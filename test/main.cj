@C
@OverflowThrowing
public unsafe func foo(x: Int64, y: Int64): Int64 {
    x + y
}

unsafe main() {
    let res = foo(2, 3)
    println(res)
    return 0
}