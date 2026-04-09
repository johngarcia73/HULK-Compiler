; ModuleID = 'HULK'
source_filename = "HULK"

declare ptr @_concat(ptr, ptr)

define i32 @main() {
entry:
  %a = alloca i32, align 4
  store i32 4, ptr %a, align 4
  %b = alloca i32, align 4
  store i32 4, ptr %b, align 4
  %u = alloca i32, align 4
  store i32 5, ptr %u, align 4
  %i = alloca i32, align 4
  store i32 0, ptr %i, align 4
  %a1 = load i32, ptr %a, align 4
  %b2 = load i32, ptr %b, align 4
  %cmp = icmp eq i32 %a1, %b2
  ret i32 0
}
