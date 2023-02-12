/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2023 katursis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef PAWNCMD_COMMAND_H_
#define PAWNCMD_COMMAND_H_

using PublicPtr = std::shared_ptr<ptl::Public>;

class Command {
 public:
  Command(const PublicPtr &pub, unsigned int flags = 0, bool is_alias = false)
      : public_{pub}, flags_{flags}, is_alias_{is_alias} {}

  inline const PublicPtr &GetPublic() const { return public_; }

  inline unsigned int GetFlags() const { return flags_; }

  inline void SetFlags(unsigned int flags) { flags_ = flags; }

  inline bool IsAlias() const { return is_alias_; }

 private:
  PublicPtr public_;
  unsigned int flags_{};
  bool is_alias_{};
};

#endif  // PAWNCMD_COMMAND_H_
