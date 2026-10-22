
/*
 *
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 */

#include "command.h"
#include "tokenizer.h"
#include <cctype>
#include <fcntl.h>
#include <glob.h>
#include <iostream>
#include <signal.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <wordexp.h>

bool showTable = false;

void parseError(std::string msg, SimpleCommand *currentSimpleCommand) {
  fprintf(stderr, "Parse error: %s\n", msg.c_str());
  Command::_currentCommand.insertSimpleCommand(currentSimpleCommand);
  Command::_currentCommand.clear();
}

bool readWord(std::vector<Token> &tokens, std::string &result, size_t &i) {
  std::string word = tokens[i].value;
  if (word[0] != '"' && word[0] != '\'') {
    result = word;
    return true;
  } else {
    char quote = word[0];
    if (word.size() >= 2 && word[word.size() - 1] == quote) {
      result = word.substr(1, word.size() - 2);
      return true;
    } else {
      std::string combinedWord = word.substr(1);
      i++;
      while (i < tokens.size() && tokens[i].type != TOKEN_EOF) {
        std::string nextWord = tokens[i].value;
        if (nextWord[nextWord.size() - 1] == quote) {
          combinedWord += " " + nextWord.substr(0, nextWord.size() - 1);
          result = combinedWord;
          return true;
        } else {
          combinedWord += " " + nextWord;
        }
        i++;
      }
      return false; // No closing quote found
    }
  }
}

void parse(std::vector<Token> &tokens) {
  if (tokens[0].type == TOKEN_EOF) {
    return;
  }
  SimpleCommand *currentSimpleCommand = new SimpleCommand();

  for (size_t i = 0; i < tokens.size(); i++) {
    switch (tokens[i].type) {
    case TOKEN_COMMAND:
    case TOKEN_ARGUMENT: {
      std::string word;
      if (!readWord(tokens, word, i)) {
        parseError("Unmatched quote in argument.", currentSimpleCommand);
        return;
      }
      currentSimpleCommand->insertArgument(strdup(word.c_str()));
      break;
    }
    case TOKEN_EOF:
      if (currentSimpleCommand->_numberOfArguments > 0) {
        Command::_currentCommand.insertSimpleCommand(currentSimpleCommand);
      } else {
        if (Command::_currentCommand._numberOfSimpleCommands > 0) {
          parseError("Missing Command after pipe operator '|'.",
                     currentSimpleCommand);
          return;
        } else {
          parseError("Missing Command", currentSimpleCommand);
          return;
        }
      }

      break;
    case TOKEN_PIPE:
      if (currentSimpleCommand->_numberOfArguments > 0) {
        Command::_currentCommand.insertSimpleCommand(currentSimpleCommand);
        currentSimpleCommand = new SimpleCommand();
      } else {
        parseError("Missing Command before pipe operator '|'.",
                   currentSimpleCommand);
        return;
      }
      break;
    case TOKEN_REDIRECT:
    case TOKEN_INPUT:
    case TOKEN_ERROR:
    case TOKEN_APPEND:
    case TOKEN_REDIRECT_AND_ERROR: {

      TokenType type = tokens[i].type;
      if (i + 1 >= tokens.size() || tokens[i + 1].type != TOKEN_ARGUMENT) {
        parseError("Missing file name after redirection operator.",
                   currentSimpleCommand);
        return;
      }

      bool duplicate = false;
      if (type == TOKEN_REDIRECT || type == TOKEN_APPEND) {
        duplicate = (Command::_currentCommand._outFile != 0) || (Command::_currentCommand._out_error != 0);
      } else if (type == TOKEN_INPUT) {
        duplicate = (Command::_currentCommand._inputFile != 0);
      } else if (type == TOKEN_ERROR) {
        duplicate = (Command::_currentCommand._errFile != 0);
      } else if (type == TOKEN_REDIRECT_AND_ERROR) {
        duplicate = (Command::_currentCommand._errFile != 0) || (Command::_currentCommand._outFile != 0);
      }

      if (duplicate) {
        parseError("Duplicate redirection operator.", currentSimpleCommand);
        return;
      }

      char *file = strdup(tokens[i + 1].value.c_str());
      switch (type) {
      case TOKEN_REDIRECT:
        Command::_currentCommand._outFile = file;
        break;
      case TOKEN_INPUT:
        Command::_currentCommand._inputFile = file;
        break;
      case TOKEN_ERROR:
        Command::_currentCommand._errFile = file;
        break;
      case TOKEN_APPEND:
        Command::_currentCommand._outFile = file;
        Command::_currentCommand._append = 1;
        break;
      case TOKEN_REDIRECT_AND_ERROR:
        Command::_currentCommand._errFile = file;
        Command::_currentCommand._append = 1;
        Command::_currentCommand._out_error = 1;
        break;
      default:
        break;
      }
      i++;

      break;
    }
    case TOKEN_BACKGROUND:
      if (tokens[i + 1].type == TOKEN_EOF &&
          currentSimpleCommand->_numberOfArguments > 0) {
        Command::_currentCommand._background = 1;
      } else {
        parseError("Background operator '&' must be at the end of the command.",
                   currentSimpleCommand);
        return;
      }
      break;
    default:
      parseError("Unexpected token.", currentSimpleCommand);
      return;
    }
  }

  Command::_currentCommand.execute();
}

SimpleCommand::SimpleCommand() {
  _numberOfAvailableArguments = 5;
  _numberOfArguments = 0;
  _arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
}

void SimpleCommand::insertArgument(char *argument) {
  if (_numberOfAvailableArguments == _numberOfArguments + 1) {
    _numberOfAvailableArguments *= 2;
    _arguments = (char **)realloc(_arguments,
                                  _numberOfAvailableArguments * sizeof(char *));
  }

  _arguments[_numberOfArguments] = argument;
  _arguments[_numberOfArguments + 1] = NULL;

  _numberOfArguments++;
}

Command::Command() {
  _numberOfAvailableSimpleCommands = 1;
  _simpleCommands = (SimpleCommand **)malloc(_numberOfAvailableSimpleCommands *
                                             sizeof(SimpleCommand *));

  _numberOfSimpleCommands = 0;
  _outFile = 0;
  _inputFile = 0;
  _errFile = 0;
  _background = 0;
  _append = 0;
  _out_error = 0;
}

void Command::insertSimpleCommand(SimpleCommand *simpleCommand) {
  if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands) {
    _numberOfAvailableSimpleCommands *= 2;
    _simpleCommands = (SimpleCommand **)realloc(
        _simpleCommands,
        _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
  }

  _simpleCommands[_numberOfSimpleCommands] = simpleCommand;
  _numberOfSimpleCommands++;
}

void Command::clear() {
  for (int i = 0; i < _numberOfSimpleCommands; i++) {
    for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++) {
      free(_simpleCommands[i]->_arguments[j]);
    }

    free(_simpleCommands[i]->_arguments);
    delete _simpleCommands[i];
  }

  if (_outFile) {
    free(_outFile);
  }

  if (_inputFile) {
    free(_inputFile);
  }

  if (_errFile) {
    free(_errFile);
  }

  _numberOfSimpleCommands = 0;
  _outFile = 0;
  _inputFile = 0;
  _errFile = 0;
  _background = 0;
  _append = 0;
  _out_error = 0;
}

void Command::print() {
  printf("\n\n");
  printf("              COMMAND TABLE                \n");
  printf("\n");
  printf("  #   Simple Commands\n");
  printf("  --- ----------------------------------------------------------\n");

  for (int i = 0; i < _numberOfSimpleCommands; i++) {
    printf("  %-3d ", i);
    for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++) {
      printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
    }
    printf("\n");
  }

  printf("\n\n");
  printf("  Output       Input        Error        Err&Out       Background\n");
  printf(
      "  ------------ ------------ ------------ ------------ ------------\n");
  printf("  %-12s %-12s %-12s %-12s %-12s\n", _outFile ? _outFile : "default",
         _inputFile ? _inputFile : "default", _errFile ? _errFile : "default",
         _out_error == 1 ? _errFile : "default", _background ? "YES" : "NO");
  printf("\n\n");
}

bool redirectTo(const char *file, int flags, int desc) {
  int fd = open(file, flags, 0644);
  if (fd < 0) {
    perror(file);

    return false;
  }
  dup2(fd, desc);
  close(fd);
  return true;
}

void restoreAndClose(int defaultin, int defaultout, int defaulterr) {
  dup2(defaultin, 0);
  dup2(defaultout, 1);
  dup2(defaulterr, 2);
  close(defaultin);
  close(defaultout);
  close(defaulterr);
}

void Command::execute() {
  if (showTable) {
    print();
  }

  if (_numberOfSimpleCommands == 1) {
    if (strcmp(_simpleCommands[0]->_arguments[0], "exit") == 0) {
      printf("Good bye!!\n");
      exit(0);
    }
    if (strcmp(_simpleCommands[0]->_arguments[0], "cd") == 0) {
      const char *dir = (_simpleCommands[0]->_arguments[1])
                            ? _simpleCommands[0]->_arguments[1]
                            : getenv("HOME");
      if (dir == NULL || chdir(dir) != 0) {
        perror("cd");
      }
      clear();
      return;
    }
  }

  pid_t lastPid = -1;
  fflush(stdout);

  int defaultin = dup(0);
  int defaultout = dup(1);
  int defaulterr = dup(2);

  if (_inputFile) {
    if (!redirectTo(_inputFile, O_RDONLY, 0)) {
      restoreAndClose(defaultin, defaultout, defaulterr);
      clear();
      return;
    }
  }

  if (_outFile) {
    if (!redirectTo(_outFile,
                    O_WRONLY | O_CREAT | (_append ? O_APPEND : O_TRUNC), 1)) {
      restoreAndClose(defaultin, defaultout, defaulterr);
      clear();
      return;
    }
  }

  if (_errFile) {
    if (!redirectTo(_errFile,
                    O_WRONLY | O_CREAT | (_out_error ? O_APPEND : O_TRUNC),
                    2)) {
      restoreAndClose(defaultin, defaultout, defaulterr);
      clear();
      return;
    }
    if (_out_error) {
      dup2(2, 1);
    }
  }

  for (int i = 0; i < _numberOfSimpleCommands; i++) {
    pid_t pid = fork();

    if (pid < 0) {
      perror("fork");
      break;
    }

    if (pid == 0) {
      close(defaultin);
      close(defaultout);
      close(defaulterr);
      signal(SIGINT, SIG_DFL);
      execvp(_simpleCommands[i]->_arguments[0], _simpleCommands[i]->_arguments);
      perror(_simpleCommands[i]->_arguments[0]);
      _exit(1);
    }

    lastPid = pid;
  }

  restoreAndClose(defaultin, defaultout, defaulterr);

  if (!_background && lastPid > 0) {
    int status;
    waitpid(lastPid, &status, 0);
  }

  clear();
}

void Command::prompt() {

  std::string input;
  while (true) {
    printf("myshell>");
    fflush(stdout);
    std::getline(std::cin, input);
    if (std::cin.eof()) {
      std::cout << std::endl;
      exit(0);
    }
    std::vector<Token> tokens = tokenize(input);
    parse(tokens);
  }
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand;

int main() {
  signal(SIGINT, SIG_IGN);
  Command::_currentCommand.prompt();
  return 0;
}
