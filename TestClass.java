import java.util.*;

public class PairCounting {
    public static int countPairs(String str) {
        // Create a HashMap to store character frequencies
        Map<Character, Integer> freqMap = new HashMap<>();
        
        // Count frequency of each character
        for (char c : str.toCharArray()) {
            freqMap.put(c, freqMap.getOrDefault(c, 0) + 1);
        }
        
        // Calculate number of pairs for each character and sum them
        int count = 0;
        for (int freq : freqMap.values()) {
            count += freq / 2;  // Integer division gives number of complete pairs
        }
        
        return count;
    }
    
    public static void main(String[] args) {
        Scanner sc = new Scanner(System.in);
        String str = sc.nextLine();
        
        System.out.println(countPairs(str));
        sc.close();
    }
}