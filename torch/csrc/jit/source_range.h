#pragma once
#include <c10/util/Exception.h>
#include <c10/util/Optional.h>

#include <algorithm>
#include <memory>
#include <iostream>
namespace torch {
namespace jit {

// Source represents a code segment. It keeps track of:
//  - text : the text of the code segment
//  - filename (optional) : if present, represents the name of the file from
//                          which the code semgemnt originated.
//  - starting_line_no : represents the line in the original file where the
//                       code segment started.
struct Source {
  explicit Source(std::string text)
      : text_(std::move(text)), filename_(c10::nullopt) {
    calc_line_start_offsets();
  }

  Source(
      std::string text,
      c10::optional<std::string> filename,
      size_t starting_line_no)
      : text_(std::move(text)),
        filename_(std::move(filename)),
        starting_line_no_(starting_line_no) {
    calc_line_start_offsets();
  }

  // Given a line number (within source_), return the byte offset of the
  // beginning of that line.
  size_t offset_for_line(size_t line) const {
    return line_starting_offsets_.at(line);
  }

  // Calculate the line (within the code segment) on which `offset` resides.
  size_t lineno_for_offset(size_t offset) const {
    return std::upper_bound(
               line_starting_offsets_.begin(),
               line_starting_offsets_.end(),
               offset) -
        line_starting_offsets_.begin() - 1;
  }

  // Calculate the line (within the original source file, if present) on which
  // `lineno` resides.
  size_t lineno_to_source_lineno(size_t lineno) const {
    if (filename_) {
      return lineno + starting_line_no_;
    } else {
      return lineno;
    }
  }

  const std::string& text() const {
    return text_;
  }

  const c10::optional<std::string>& filename() const {
    return filename_;
  }

  size_t starting_line_no() const {
    return starting_line_no_;
  }

 private:
  void calc_line_start_offsets() {
    size_t pos = 0;
    do {
      line_starting_offsets_.push_back(pos);
      pos++;
    } while ((pos = text_.find('\n', pos)) != std::string::npos);
  }
  std::string text_;
  c10::optional<std::string> filename_;
  // If filename_ is not present, starting_line_no_ is don't care
  size_t starting_line_no_;
  // Starting offsets for lines into the source. e.g. line 0 starts at
  // line_starting_offsets_[0], etc.
  std::vector<size_t> line_starting_offsets_;
};

// A SourceRange is a view into a Source, that points to a subset of the source,
// specified by `start` and `end` byte offsets into the source text.
struct CAFFE2_API SourceRange {
  SourceRange(std::shared_ptr<Source> source_, size_t start_, size_t end_)
      : source_(std::move(source_)), start_(start_), end_(end_) {}
  explicit SourceRange(std::string string_range)
      : source_(std::make_shared<Source>(std::move(string_range))),
        start_(0),
        end_(source_->text().size()) {}

  const std::string text() const {
    return source_->text().substr(start(), end() - start());
  }
  size_t size() const {
    return end() - start();
  }
  static const size_t CONTEXT = 10;
  void highlight(std::ostream& out) const;
  const std::shared_ptr<Source>& source() const {
    return source_;
  }
  size_t start() const {
    return start_;
  }
  size_t end() const {
    return end_;
  }
  std::string str() const {
    std::stringstream ss;
    highlight(ss);
    return ss.str();
  }
  std::string wrapException(
      const std::exception& e,
      const std::string& additional = "") {
    std::stringstream msg;
    std::string what;
    auto c10_error = dynamic_cast<const c10::Error*>(&e);
    if (c10_error) {
      what = c10_error->msg_without_backtrace();
    } else {
      what = e.what();
    }
    msg << "\n" << what << ":\n";
    if (!additional.empty()) {
      msg << additional << ":\n";
    }
    highlight(msg);
    return msg.str();
  }
  void wrapAndRethrowException(
      const std::exception& e,
      const std::string& additional = "") {
    throw std::runtime_error(wrapException(e, additional));
  }

 private:
  std::shared_ptr<Source> source_;
  size_t start_;
  size_t end_;
};

inline std::ostream& operator<<(std::ostream& out, const SourceRange& range) {
  range.highlight(out);
  return out;
}

} // namespace jit
} // namespace torch
