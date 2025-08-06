@C
@OverflowThrowing
public unsafe func foo(x: Int64, y: Int64): Int64 {
    x + y
}

public class A <: Object {
    private var x: Int64 = 0

    public init() {
        A(1)
    }

    public init(x: Int64) {
        this.x = x
    }

    public func hello() {
        println(x)
    }
}

func foo1(x: ?A) {
    match (x) {
        case Some(v) => v.hello()
        case None => ()
    }
}

func foo2() {
    let lam1 = {=> 1}
    let lam2 = {x: Int64 => x * 2}
    let lam3 = {x: Int64, y: Int64 => x + y}
    lam1() + lam2(1) + lam3(2, 3)
}

unsafe main() {
    let res = foo(2, 3)
    println(res)
    foo1(A())
    foo2()
    return 0
}