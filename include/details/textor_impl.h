#ifndef __TEXTOR_IMPL_H_
#define __TEXTOR_IMPL_H_

#include <error.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include "spdlog/spdlog.h"

#define CTRL_KEY(k) ((k)&0x1f)

namespace textor {
static struct termios _orig_termios;

inline void disable_raw_mode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &_orig_termios) == -1) {
    SPDLOG_ERROR("tcgetattr");
  }
}

void enable_raw_mode() {
  if (tcgetattr(STDIN_FILENO, &_orig_termios) == -1) {
    SPDLOG_ERROR("tcgetattr");
  }
  atexit(disable_raw_mode);

  struct termios raw = _orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag &= ~(CS8);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) return;
}

inline void editor::read_key__() {
  int nread;
  while ((nread = read(STDIN_FILENO, &key_, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) SPDLOG_ERROR("read error");
  }

  if (key_ == '\x1b') {
    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1) return;
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return;

    if (seq[0] == '[') {
      switch (seq[1]) {
        case 'A':
          key_ = ARROW_UP;
          return;
        case 'B':
          key_ = ARROW_DOWN;
          return;
        case 'C':
          key_ = ARROW_RIGHT;
          return;
        case 'D':
          key_ = ARROW_LEFT;
          return;
      }
    }
  }
}

void editor::process_key__() {
  read_key__();
  switch (key_) {
    case '\r':
      insert_row__();
      break;
    case BACKSPACE:
    case CTRL('h'):
      delete_char__();
    case CTRL('l'):
      break;
    case CTRL_KEY('q'):
      if (dirty_ && quit_times_) {
        status_msg__(
            "WARNING!!! Files had modifed. Please press Ctrl+S to save or "
            "Ctrl+Q to quit without save!");
        quit_times_--;
        return;
      }
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
    case CTRL('s'):
      save__();
      break;
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      move_cursor__();
      break;
    default:
      insert_char__();
      break;
  }
}

inline void editor::flush__() {
  scroll__();
  append_buf__("\x1b[?25l");
  append_buf__("\x1b[H");
  draw_rows__();
  status_bar__();
  draw_msg__();
  char buf[32];
  snprintf(buf, sizeof buf, "\x1b[%d;%dH", (cx_ - rowoff_) + 1,
           (cy_ - coloff_) + 1);
  append_buf__(buf);
  append_buf__("\x1b[?25h");
  write(STDOUT_FILENO, abuf_.c_str(), abuf_.size());
  abuf_.clear();
}

void editor::draw_rows__() {
  for (int i = 0; i < rows_; i++) {
    int filerow = i + rowoff_;
    if (filerow >= numrows_) {
      if (i == rows_ / 3 && !numrows_) {
        std::string welcome("Textor editor -- version ");
        welcome += TEXTOR_VERSION;
        if (welcome.size() > cols_) welcome.resize(cols_);
        int padding = (cols_ - welcome.size()) / 2;
        if (padding) {
          append_buf__("~");
          padding--;
        }
        while (padding--) append_buf__(" ");
        append_buf__(welcome);
      } else {
        append_buf__("~");
      }
    } else {
      int len = text_[filerow].size() - coloff_;
      if (len < 0) {
        len += coloff_;
        append_buf__(text_[filerow].substr(len, 0));
        return;
      }
      if (len > cols_) len = cols_;
      append_buf__(text_[filerow].substr(coloff_, len));
    }
    append_buf__("\x1b[K");
    append_buf__("\r\n");
  }
}

inline int editor::get_window_size__() {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return get_cursor_position__();
  } else {
    cols_ = ws.ws_col;
    rows_ = ws.ws_row - 2; /* 腾出状态栏 */
    return 0;
  }
}

int editor::get_cursor_position__() {
  char buf[32];
  int i;

  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

  for (i = 0; i < sizeof buf - 1; ++i) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
  }

  buf[i] = '\0';

  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows_, cols_) != 2) return -1;
  return 0;
}

void editor::move_cursor__() {
  switch (key_) {
    case ARROW_UP:
      if (cx_ != 0) cx_--;
      break;
    case ARROW_DOWN:
      if (cx_ < numrows_ - 1) cx_++;
      if (!text_.empty() && cy_ > text_[cx_].size()) cy_ = text_[cx_].size();
      break;
    case ARROW_LEFT:
      if (cy_ > 0)
        cy_--;
      else if (cx_ != 0) {
        cx_--;
        cy_ = text_[cx_].size();
      }
      break;
    case ARROW_RIGHT:
      if (!text_.empty() && cy_ < text_[cx_].size())
        cy_++;
      else if (cx_ < numrows_ - 1) {
        cy_ = 0;
        cx_++;
      }
      break;
  }
}

void editor::open__(char *fpath) {
  std::ifstream ifs;
  ifs.open(fpath, std::ifstream::in);
  if (ifs.fail()) SPDLOG_ERROR("open file failed");
  while (!ifs.eof()) {
    std::string str;
    getline(ifs, str);
    text_.push_back(str);
  }
  numrows_ = text_.size();
  ifs.close();
  filename_.clear();
  filename_ = fpath;
}

editor::editor()
    : cx_(0),
      cy_(0),
      numrows_(0),
      rowoff_(0),
      coloff_(0),
      dirty_(false),
      quit_times_(2) {
  spdlog::set_pattern("*** [%H:%M:%S %z] [thread %t] %v at %@ ***");
  if (get_window_size__() == -1) SPDLOG_ERROR("get_window_size__() error");
}

inline void editor::append_buf__(const std::string &s) { abuf_ += s; }

void editor::scroll__() {
  if (cx_ < rowoff_)
    rowoff_ = cx_;
  else if (cx_ >= rowoff_ + rows_)
    rowoff_ = cx_ - rows_ + 1;
  if (cy_ < coloff_)
    coloff_ = cy_;
  else if (cy_ >= coloff_ + cols_)
    coloff_ = cy_ - cols_ + 1;
}

void editor::status_bar__() {
  append_buf__("\x1b[7m"); /* 颜色反转 */
  std::string stats((filename_.empty() ? "[New File]" : filename_) +
                    " - total: " + std::to_string(numrows_) +
                    " lines current: (" + std::to_string(cx_) + ", " +
                    std::to_string(cy_) + ")" + (dirty_ ? " (modified)" : ""));
  append_buf__(stats);
  for (int i = 0; i < cols_ - stats.size(); ++i) append_buf__(" ");
  append_buf__("\x1b[m");
  append_buf__("\r\n");
}

void editor::status_msg__(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  char msg[80];
  vsnprintf(msg, sizeof msg, fmt, ap);
  va_end(ap);
  status_msg_ = msg;
  msg_time_ = time(nullptr);
}

void editor::draw_msg__() {
  append_buf__("\x1b[K"); /* 清除消息框 */
  if (status_msg_.size() > cols_) status_msg_.resize(cols_);
  if (!status_msg_.empty() && time(nullptr) - msg_time_ < 5)
    append_buf__(status_msg_);
}

void editor::insert_char__() {
  if (cx_ == numrows_) {
    text_.push_back("");
    numrows_++;
  }
  if (cy_ < 0 || cy_ > text_[cx_].size()) cy_ = text_[cx_].size();
  text_[cx_].insert(cy_, 1, key_);
  cy_++;
  if (!dirty_) dirty_ = true;
}

void editor::delete_char__() {
  if (cy_ < 0 || cy_ > text_[cx_].size()) return;
  if (cy_ == 0) {
    if (cx_ == 0) return;
    while (text_.empty() && cx_) {
      cx_--;
      numrows_--;
    }
    cx_--;
    cy_ = text_[cx_].size();
    text_[cx_] += text_[cx_ + 1];
    text_.erase(text_.begin() + cx_ + 1);
  } else
    text_[cx_].erase(--cy_, 1);
  if (!dirty_) dirty_ = true;
}

void editor::insert_row__() {
  if (cy_ < 0 || cy_ >= text_[cx_].size()) return;
  std::string tail(text_[cx_].begin() + cy_, text_[cx_].end());
  text_[cx_].erase(text_[cx_].begin() + cy_ + 1, text_[cx_].end());
  text_[cx_].resize(cy_);
  cx_++;
  text_.insert(text_.begin() + cx_, tail);
  numrows_++;
  cy_ = 0;
  if (!dirty_) dirty_ = true;
}

void editor::save__() {
  if (filename_.empty()) prompt__("Save as : %s (Esc to cancel)");
  if (filename_.empty()) {
    status_msg__("Save aborted");
    return;
  }
  std::ofstream ofs;
  ofs.open(filename_.c_str(), std::ifstream::out);
  if (ofs.fail()) SPDLOG_ERROR("open file failed");
  for (auto row : text_) ofs << row + "\n";
  ofs.close();
  status_msg__("Save to %s successful!", filename_.c_str());
  dirty_ = false;
  quit_times_ = 2;
}

void editor::prompt__(const std::string &prompt) {
  while (1) {
    status_msg__(prompt.c_str(), filename_.c_str());
    flush__();

    read_key__();
    if (key_ == BACKSPACE && !filename_.empty()) {
      filename_.pop_back();
    } else if (key_ == '\x1b') {
      filename_.clear();
      return;
    } else if (key_ == '\r') {
      if (!filename_.empty()) return;
    } else if (!iscntrl(key_) && key_ < 128) {
      filename_ += key_;
    }
  }
}

int editor::run(char *fpath = nullptr) {
  char c;
  enable_raw_mode();
  if (fpath != nullptr) open__(fpath);
  status_msg__("HELP: Ctrl+S = save | Ctrl+Q = quit");
  while (1) {
    flush__();
    process_key__();
  }

  return 0;
}
}  // namespace textor

#endif  // __TEXTOR_IMPL_H_
