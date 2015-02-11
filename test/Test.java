package test;

public class Test implements Runnable {
	
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
				Thread.sleep(1000);
			} catch(Exception e) {
				e.printStackTrace();
			}
		}
	}
}
