#ifndef __TEXTOR_H_
#define __TEXTOR_H_

#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#define TEXTOR_VERSION "0.0.1"

namespace textor {

enum key { BACKSPACE = 127, ARROW_UP, ARROW_DOWN, ARROW_LEFT, ARROW_RIGHT };

struct editor {
 private:
  /* 窗口大小 */
  int cols_;
  int rows_;

  /* 光标位置 */
  int cx_;
  int cy_;

  /* 按键 */
  unsigned char key_;

  /* buffer */
  std::string abuf_;

  /* 显示的内容和行数 */
  std::vector<std::string> text_;
  int numrows_; /* 多行 */
  int rowoff_;  /* 垂直滚动 */
  int coloff_;  /* 水平滚动 */

  /* 渲染 */
  std::vector<std::string> render;

  /* 文件名 */
  std::string filename_;

  /* 消息 */
  std::string status_msg_;
  std::time_t msg_time_;

  /* 是否修改 */
  bool dirty_;
  int quit_times_;

  /* 刷新屏幕 */
  void flush__();

  /* 打开文件 */
  void open__(char *);

  /* 读入一个字符 */
  void read_key__();

  /* 处理按键 */
  void process_key__();

  /* 画出编辑窗口 */
  void draw_rows__();

  /* 窗口属性设置 */
  int get_window_size__();
  int get_cursor_position__();

  /* 移动光标 */
  void move_cursor__();

  /* 填充 buffer */
  void append_buf__(const std::string &s);

  /* 滚动 */
  void scroll__();

  /* 状态栏 */
  void status_bar__();

  /* 消息处理 */
  void status_msg__(const char *, ...);
  void draw_msg__();

  /* 插入字符 */
  void insert_char__();

  /* 删除字符 */
  void delete_char__();

  /* 插入一行 */
  void insert_row__();

  /* 存盘 */
  void save__();

  /* 另存为文件名 */
  void prompt__(const std::string &);

 public:
  editor();
  ~editor() = default;
  int run(char *);
};
}  // namespace textor

#endif  // __TEXTOR_H_

#include "details/textor_impl.h"
