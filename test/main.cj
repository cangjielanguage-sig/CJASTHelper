package a.b.c

import std.fs.File
import std.collection.*
import std.io.InputStream as IS
internal import std.{math.abs, binary.*, env.getCommandLine as getArgs}
import std.fs.Directory
import std.time

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

func foo3() {
    let v = 1
    match (v) {
        case b: Bool => b
        case _ =>
            throw Exception("exception")
    }
}

func foo4(p0: Int64, p1: ?Int64, p2: Option<Int64>,
    p3: (Int64), p4: (Int64, Int64),
    p5: () -> Unit, p6: (Int64) -> Int64, p7: (Int64, Int64) -> Int64,
    p8: VArray<Int64, $5>
) {}

class MyArray<T> {
    let data: Array<T>

    public init() {
        data = Array<T>()
    }

    public operator func [](i: Int64): T {
        data[i]
    }
    public operator func [](i: Int64, val: T): Unit {
        data[i] = val
    }

}

func foo5<T1, T2>(x: T1, y: T2): Unit where T1 <: A, T2 <: Object {
    ()
}

func foo6(x: Int64, b: Bool) {
    let y = x
    x + y
    !b
    x - y
    x << 1
    x > y
    var z = y ** 2
    z++
    z--
}

func foo7(): time.DayOfWeek {
    time.DayOfWeek.Sunday
}

func foo8(): Unit {
    var i = 0
    while (i < 10) {
        if (i == 0) {
            continue
        }
        if (i == 9) {
            break
        }
        i = i + 1
    }
    i = 0
    do {
        if (i % 2 == 0) {
            i = i + 1
            continue
        } else if (i > 7) {
            break
        }
    } while (i < 10)

    let v0: ?Int64 = 10
    if (let Some(v) <- v0) {
        println(v)
    }

    while (let Some(v) <- v0) {
        println(v)
        if (v == 10) {
            break
        }
    }

    do {
        if (let Some(v) <- v0) {
            println(v)
            if (v == 10) {
                break
            }
        }
    } while (true)
}

func foo(x: Int64) { x }

func foo9() {

    for (i in 0..10) {
        foo(i)
    }
    
    let size = 10
    let s = 1
    for (j in s..size) {
        foo(j)
    }

    for (val in [0, 1, 2, 3]) {
        foo(val)
    }

    let mp = HashMap<Int64, String>()
    for ((k, v) in mp) {
        foo(k)
    }

    for (x in "hello world") {
        println(x)
    }
}

class B {
    var data: Int64 = 0
    public prop a: Int64 {
        get() {1}
    }

    public mut prop b: Int64 {
        get() { data + 1}
        set(v) { this.data = v + 1 }
    }
}

interface I0 {
    func foo(): Unit
    func foo(x: Int64): Int64

    prop bar: Int64
}

struct S <: I0 {
    public func foo(): Unit {
        println("S.foo()")
    }

    public func foo(x: Int64): Int64 {
        println("S.foo(x)")
        return x
    }

    public prop bar: Int64 {
        get() {
            println("S.bar.get()")
            return 42
        }
    }
}

enum E <: I0 {
    | A
    | B
    | C(Int64)
    | D(Int64, S)

    public func foo(): Unit {
        println("E.foo()")
    }

    public func foo(x: Int64): Int64 {
        println("E.foo(x)")
        return x
    }

    public prop bar: Int64 {
        get() {
            println("E.bar.get()")
            return 42
        }
    }
}

unsafe func entry() {
    let res = foo(2, 3)
    println(res)
    foo1(A())
    foo2()
    foo3()
    return 0
}