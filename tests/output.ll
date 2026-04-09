; ModuleID = 'HULK'
source_filename = "HULK"

declare ptr @_concat(ptr, ptr)

define i32 @main() {
entry:
  %x = alloca i32, align 4
  store i32 5, ptr %x, align 4
  %x1 = load i32, ptr %x, align 4
  %add = add i32 %x1, 5
  ret i32 0
}
