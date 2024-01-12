0-4 Path Enumeration
5-14 Pointers

test0 - standard .txt file
test1 - given test case in specs
test2 - nested folders
test3 - tree-like structure with directory and files
test4 - similar to test1 but with non-.txt file extensions

test5 - file uses x < 1 DP                                      (direct pointer 0 must not be a nonzero value)
test6 - file uses 1 < x < 2 DP                                  (direct pointers 0 and 1 must not be a nonzero value)
test7 - file uses 12 DP                                         (all direct pointers must not contain zero values)
test8 - file uses 13 DP                                         (singly indirect pointer must not be zero)
test9 - file uses 12 < x < 13 DP                                (singly indirect pointer must not be zero)
test10 - file uses 12 + 1024 DP                                 (singly indirect block must not contain zero values)
test11 - file uses 12 + 1024 DP < x < 12 + 1024 + 1024^2 DP     (doubly indirect pointer must not be a zero)
test11 - file uses 12 + 1024 + 1024^2 DP                        (doubly indirect block must not contain zero values)
test12 - file uses 12 + 1024 + 1024^2 DP < x < 12 + 1024 + 1024^2 + 1024^3 DP      (triply indirect pointer must not be a zero)
test13 - file uses 12 + 1024 + 1024^2 + 1024^3 DP               (MAX FILE SIZE: triply indirect block must not contain zero values)
test15 - Pictures of Ahri (waifu, goddess, pool party skin).