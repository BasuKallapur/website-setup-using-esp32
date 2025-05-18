import java.util.*;

public class FruitBoxSelection {
    static int numOfWays(int N, int arr[], int K) {
        int[][] dp = new int[N + 1][K + 1];
        dp[0][0] = 1;
        for (int i = 1; i <= N; i++) {
            for (int j = 0; j <= K; j++) {
                dp[i][j] = dp[i - 1][j];
                if (j >= arr[i - 1]) {
                    dp[i][j] += dp[i - 1][j - arr[i - 1]];
                }
            }
        }
        System.out.println("DP Table:");
        for (int[] row : dp) {
            System.out.println(Arrays.toString(row));
        }
        return dp[N][K];
    }

    public static void main(String[] args) {
        Scanner sc = new Scanner(System.in);
        int N = sc.nextInt();
        int[] arr = new int[N];
        for (int i = 0; i < N; i++) {
            arr[i] = sc.nextInt();
        }
        int K = sc.nextInt();
        System.out.println(numOfWays(N, arr, K));
    }
} 