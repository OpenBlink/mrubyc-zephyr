# SPDX-License-Identifier: BSD-3-Clause

begin
  raise RuntimeError, "handled exception"
rescue RuntimeError
  puts "Rescued exception"
end

raise RuntimeError, "unhandled test exception"
