import java.util.LinkedList;
import java.util.List;

public class MonitorBuffer {
    // Shared buffer
    private final List<Integer> buffer = new LinkedList<>();

    private volatile boolean exit = false;

    private static final int NUM_PRODUCERS = 2;
    private static final int NUM_CONSUMERS = 2;

    class Producer extends Thread {
        private final int id;
        private int count = 0;

        Producer(int id) {
            this.id = id;
        }

        @Override
        public void run() {
            int product = 0;
            while (!exit) {
                synchronized (buffer) {
                    buffer.add(product);
                    System.out.println("Producer " + id + " produced: " + product);
                    product++;
                }
                if (++count % 2 == 0) { // Sleep every two additions
                    try {
                        Thread.sleep(1000);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
            }
        }
    }

    class Consumer extends Thread {
        private final int id;
        private int count = 0;

        Consumer(int id) {
            this.id = id;
        }

        @Override
        public void run() {
            while (!exit) {
                Integer product = null;
                synchronized (buffer) {
                    if (!buffer.isEmpty()) {
                        product = buffer.remove(0);
                        System.out.println("Consumer " + id + " consumed: " + product);
                    }
                }
                if (product != null && (++count % 2 == 0)) {
                    try {
                        Thread.sleep(1000);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
            }
        }
    }

    public void start() {
        for (int i = 0; i < NUM_PRODUCERS; i++) {
            new Producer(i).start();
        }

        for (int i = 0; i < NUM_CONSUMERS; i++) {
            new Consumer(i).start();
        }

        System.out.println("Press Enter key to terminate...");
        try {
            System.in.read();
            exit = true;
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    public static void main(String[] args) {
        new MonitorBuffer().start();
    }
}
