; ModuleID = 'function_pointers01.c'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: nounwind uwtable
define i32 @sum(i32 %count) #0 {
  %1 = alloca i32, align 4
  %sum = alloca i32, align 4
  %i = alloca i32, align 4
  store i32 %count, i32* %1, align 4
  store i32 0, i32* %sum, align 4
  store i32 1, i32* %i, align 4
  br label %2

; <label>:2                                       ; preds = %10, %0
  %3 = load i32, i32* %i, align 4
  %4 = load i32, i32* %1, align 4
  %5 = icmp sle i32 %3, %4
  br i1 %5, label %6, label %13

; <label>:6                                       ; preds = %2
  %7 = load i32, i32* %i, align 4
  %8 = load i32, i32* %sum, align 4
  %9 = add nsw i32 %8, %7
  store i32 %9, i32* %sum, align 4
  br label %10

; <label>:10                                      ; preds = %6
  %11 = load i32, i32* %i, align 4
  %12 = add nsw i32 %11, 1
  store i32 %12, i32* %i, align 4
  br label %2

; <label>:13                                      ; preds = %2
  %14 = load i32, i32* %sum, align 4
  ret i32 %14
}

; Function Attrs: nounwind uwtable
define i32 @main() #0 {
  %1 = alloca i32, align 4
  %sum_pointer = alloca i32 (i32)*, align 8
  %sum = alloca i32, align 4
  store i32 0, i32* %1, align 4
  store i32 (i32)* @sum, i32 (i32)** %sum_pointer, align 8
  %2 = load i32 (i32)*, i32 (i32)** %sum_pointer, align 8
  %3 = call i32 %2(i32 5)
  store i32 %3, i32* %sum, align 4
  ret i32 0
}

attributes #0 = { nounwind uwtable "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = !{!"clang version 3.8.0-2ubuntu4 (tags/RELEASE_380/final)"}
