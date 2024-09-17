import java.security.MessageDigest;
import java.util.*;

public class MerkleTrees {
    //List of entries
    private static List<String> entries;
    private static List<String> leaves;
    // Merkle Root
    private String root;

    //constructor
    public MerkleTrees(List<String> entries) {
        this.entries = entries;
        leaves = getLeavesHash();
        root = "";
    }

    /**
     * execute full merkle_tree and set root.
     */
    public void merkle_tree() {
        List<String> tempList = new ArrayList<>(leaves);
        List<String> newList = getNewList(tempList);

        //Execute the loop until only one hash value is left
        while (newList.size() != 1) {
            newList = getNewList(newList);
        }

        this.root = newList.get(0);
    }

    /**
     * execute perfect merkle_tree and return HashMap to all the node in the tree.
     */
    public Map<Integer, String> perfect_merkle_tree(int h) {
        Map<Integer, String> tree = new HashMap<>();
        List<String> tempList = new ArrayList<>();
        List<String> newList = new ArrayList<>();
        int numLeaves = (int) Math.pow(2, h);
        int startID = numLeaves;

        for (int i = 0; i < numLeaves; i++) {
            if (i < leaves.size()) {
                tempList.add(leaves.get(i));
                tree.put(startID, tempList.get(i)); //Put the leaves nodes on the tree
            }
            else {
                tree.put(startID, "test" + i); //Add dummy nodes
            }
            startID++;
        }

        h--;
        startID = (int) Math.pow(2, h);

        newList = getNewList(tempList);
        for (String valueHash : newList) {
            tree.put(startID, valueHash);
            startID++;
        }

        //Execute the loop until only one hash value is left
        while (newList.size() != 1) {
            h--;
            startID = (int) Math.pow(2, h);
            newList = getNewList(newList);
            for (String valueHash : newList) {
                tree.put(startID, valueHash);
                startID++;
            }
        }
        this.root = newList.get(0);

        return tree;
    }

    public Map<Integer, String> swapped_perfect_merkle_tree(Map<Integer, String> tree) {
        Map<Integer, String> swapped_tree = new HashMap<>();

        // Iterate through consecutive pairs of keys
        for (int i = 2; i < tree.size(); i++) {
            int currentKey = i;
            int nextKey = ++i;
            swapped_tree.put(currentKey, tree.get(nextKey));
            swapped_tree.put(nextKey, tree.get(currentKey));
        }
        return swapped_tree;
    }

    /**
     * return New Hash List.
     */
    private static List<String> getNewList(List<String> tempList) {

        List<String> newList = new ArrayList<>();
        //hashing range of pairs nodes
        for (int i = 0; i < tempList.size() - 1; i = i + 2){
            String parentHash = rStrInHex(getSHA256(tempList.get(i), tempList.get(i + 1)));
            newList.add(parentHash);
        }
        //hashing the final odd node if appeared
        if (tempList.size() % 2 == 1) {
            // sha2 hex value
            String parentHash = rStrInHex(getSHA256(tempList.get(tempList.size() - 1), tempList.get(tempList.size() - 1)));
            newList.add(parentHash);
            //System.out.println("odd: " + parentHash);
        }

        return newList;
    }

    /**
     * return Leaves Hash List.
     */
    private static List<String> getLeavesHash() {

        List<String> newList = new ArrayList<>();
        //hashing range of pairs nodes
        for (int i = 0; i < entries.size(); i++){
            String parentHash = rStrInHex(getSHA256(entries.get(i)));
            newList.add(parentHash);
        }

        return newList;
    }

    /**
     * Return hash value from the certificate
     */
    public static String getSHA256(String certificate) {
        try{

            MessageDigest md = MessageDigest.getInstance("SHA-256");
            String rStrInHex = rStrInHex(certificate);
            byte[] rStrInByte = StrInByte(rStrInHex);

            //hashing a string in Hex value (sha256)
            byte[] digest = md.digest(md.digest(rStrInByte));

            //Convert Hex to String
            //bytes to hex
            StringBuilder sb = new StringBuilder(2 * digest.length);
            for(byte b: digest) {
                sb.append(String.format("%02x", b) );
            }
            return sb.toString();
        } catch (Exception e) {
            e.printStackTrace();
        }
        return "";
    }

    /**
     * Return hash value from two leaves
     */
    public static String getSHA256(String strInHexLeft, String strInHexRight) {
        try{
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            String rStrLeftInHex = rStrInHex(strInHexLeft);
            String rStrRightInHex = rStrInHex(strInHexRight);
            String rStrcombine = rStrLeftInHex + rStrRightInHex;

            byte[] rStrInByte;
            rStrInByte = StrInByte(rStrcombine);

            //hashing a string in Hex value (sha256)
            byte[] digest = md.digest(md.digest(rStrInByte));

            //Convert Hex to String
            //bytes to hex
            StringBuilder sb = new StringBuilder(2 * digest.length);
            for(byte b: digest) {
                sb.append(String.format("%02x", b) );
            }
            return sb.toString();
        } catch (Exception e) {
            e.printStackTrace();
        }
        return "";
    }

    public String getRoot() {
        return this.root;
    }

    //reverse input hash value (str)
    private static String rStrInHex(String str){
        int index = str.length() - 2;
        StringBuilder rStr = new StringBuilder();

        while (index >= 0){
            rStr.append(str, index, index + 2);
            index -= 2;
        }
        return rStr.toString();
    }

    //Decode String hash to byte
    private static byte[] StrInByte(String strInHex){

        int index = 0;
        int j = 0;
        byte[] rStrInByte = new byte[strInHex.length()/2];
        StringBuilder rStrInHex = new StringBuilder();

        while (index < strInHex.length()){
            String tmp;
            tmp = strInHex.substring(index ,index+2);
            rStrInHex.append(tmp);

            int firstDigit = Character.digit(tmp.charAt(0), 16);
            int secondDigit = Character.digit(tmp.charAt(1), 16);
            rStrInByte[j++] = (byte) ((firstDigit << 4) + secondDigit);
            index += 2;
        }

        return rStrInByte;
    }
}
