; ModuleID = 'recursive01.c'
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

; Function Attrs: nounwind uwtable
define void @recursive(i32 %i) #0 {
  %1 = alloca i32, align 4
  %a = alloca [10 x i32], align 16
  store i32 %i, i32* %1, align 4
  %2 = load i32, i32* %1, align 4
  %3 = icmp sle i32 %2, 0
  br i1 %3, label %4, label %5

; <label>:4                                       ; preds = %0
  br label %10

; <label>:5                                       ; preds = %0
  %6 = getelementptr inbounds [10 x i32], [10 x i32]* %a, i64 0, i64 0
  store i32 1, i32* %6, align 16
  %7 = getelementptr inbounds [10 x i32], [10 x i32]* %a, i64 0, i64 100
  store i32 2, i32* %7, align 16
  %8 = load i32, i32* %1, align 4
  %9 = add nsw i32 %8, -1
  store i32 %9, i32* %1, align 4
  call void @recursive(i32 %9)
  br label %10

; <label>:10                                      ; preds = %5, %4
  ret void
}

; Function Attrs: nounwind uwtable
define i32 @main() #0 {
  %1 = alloca i32, align 4
  store i32 0, i32* %1, align 4
  call void @recursive(i32 10)
  ret i32 0
}

attributes #0 = { nounwind uwtable "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.ident = !{!0}

!0 = !{!"clang version 3.8.0-2ubuntu4 (tags/RELEASE_380/final)"}
