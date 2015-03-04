package test;

public class Test implements Runnable {


	public Test() {}

	public Test(boolean z, byte b, char c, short s, int i, float f, double d, long j, String $) {
		System.out.println("new Test(" + z + ", " + b + ", '" + c + "', " + s + ", " + i + ", " + f + ", " + d +", " + j + ", \"" + $ + "\")");
	}

	public void method(String str) {
		System.out.println("voidMethod(\"" + str + "\")");
	}

	public boolean method(boolean input) {
		System.out.println("booleanMethod(" + input + ")");
		return !input;
	}

	public byte method(byte input) {
		System.out.println("byteMethod(" + input + ")");
		return (byte) (input + 1);
	}

	public char method(char input) {
		System.out.println("charMethod('" + input + "')");
		return (char) (input + 1);
	}

	public short method(short input) {
		System.out.println("shortMethod(" + input + ")");
		return (short) (input + 1);
	}

	public int method(int input) {
		System.out.println("intMethod(" + input + ")");
		return ~input;
	}

	public float method(float input) {
		System.out.println("floatMethod(" + input + ")");
		return  input + 1;
	}

	public double method(double input) {
		System.out.println("doubleMethod(" + input + ")");
		return  input + Math.PI;
	}

	public long method(long input) {
		System.out.println("longMethod(" + input + ")");
		return  input + 10000000000000000L;
	}

	public String method(char a, char b) {
        System.out.println("stringMethod('" + a + "', '" + b + "')");
        return new String(new char[]{a, b});
    }

	public Test method(Test a) {
		System.out.println("ObjectMethod(this:" + (a == this) + ")");
		if(a != this) System.out.println(a);
		return a;
	}

	public Object method(String s, int n) {
		System.out.println("ObjectMethod(\"" + s + "\")");
		return s.substring(n);
	}

	public String method(String s, char c) {
		System.out.println("\"" + s + "\" + '" + c + "'");
		return s + c;
	}
	
	public static void main(String[] args) {
		System.out.println("hello world!");
		for(String arg: args) {
			System.out.println(arg);
		}
		Thread th = new Thread(new Test());
		//th.setDaemon(true);
		th.start(); // starts new thread, and returns
	}

	public void run() {
		for(int n=5; n-- > 0; ) {
			System.out.println(System.currentTimeMillis());
			try {
				Thread.sleep(100);
			} catch(Exception e) {
				e.printStackTrace();
			}
		}
	}
}
