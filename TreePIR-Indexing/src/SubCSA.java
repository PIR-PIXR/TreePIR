import java.io.*;
import java.util.*;

public class SubCSA {

    public static void main(String[] args) throws IOException {
        String PATH = "/home/quang/Desktop/TreePIR/SubCSA/";
        //Array of balanced/unbalanced sets
        NodesSet[] NodesSets;
        //List of all feasible sequences
        ArrayList<ArrayList<NumColor>> F;
        //List of color sequence c
        List<NumColor> c = new ArrayList<>();
        //The color sequence c = [c1,...,ch]
        ArrayList<Integer> vectorC = new ArrayList<>();

        //h is the height of a tree
        byte h = 3;
        int[] path;
        long[] pathID;

        if (args.length >= 2) {
            h = (byte) Integer.parseInt(args[0]);
            PATH = args[1] + "/";
        }
        else {
            h = h(); //Enter the height of a tree "h" from keyboard
        }

        char[] color = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q'
                , 'R','S', 'T', 'U', 'V','W', 'X', 'Y', 'Z','0', '1', '2', '3', '4', '5', '6', '7','8', '9'};

        PrintWriter logIndex = null;
        try {
            logIndex = new PrintWriter(new FileWriter(PATH + "CSAindexing_" + h + "_" + 2 + "_log.txt"));
        } catch (IOException e) {
            e.printStackTrace();
        }

        String filePath = "list_TXs_" + h + "_" + 2 + ".txt";
        List<Long> randIndices = new ArrayList<>();

        try (BufferedReader reader = new BufferedReader(new FileReader(filePath))) {
            String line;
            while ((line = reader.readLine()) != null) {
                randIndices.add(Long.parseLong(line.trim()));
            }
        } catch (IOException | NumberFormatException e) {
            e.printStackTrace();
        }

        // Testing
        for (Long j : randIndices) {
            SubIndices sb = new SubIndices(h, j);
            path = sb.Path();
            pathID = sb.PathID();

            c = sb.balancedColorSequence(h);
            Collections.sort(c);

            c.forEach(x -> vectorC.add(x.getSize()));

            //Print input including the height of a tree and a color sequence c = [c1,...,ch]
            System.out.println("*** Indexing Input:");
            System.out.println("    Tree height: h = " + h);
            System.out.println("    c = " + vectorC);


            long start = System.nanoTime(); // ************************** START *************************
            LinkedHashMap<Character, Integer> indices = sb.mapIndices(path, c);
            long end = System.nanoTime(); // ***************************** END **************************
            long elapsed = (end - start) / 1000;
            System.out.println("    Indexing q-ary Tree - Execution time in microseconds: " + elapsed);
            logIndex.println("Indexing: " + elapsed + " (us)");

            HashMap<Character, Long> NodeID = new HashMap<>();
            int k = 0;
            for (Map.Entry<Character, Integer> entry : indices.entrySet()) {
                NodeID.put(entry.getKey(), pathID[k]);
                k++;
            }

            String idxName = "color_indices_" + h + "_" + 2 + ".txt";
            saveRandIndices (color, NodeID, indices, h, idxName, j);

            c.clear();
            vectorC.clear();
        }
        logIndex.close();
    }

    private static void saveRandIndices(char[] color, HashMap<Character, Long> nodeID, LinkedHashMap<Character, Integer> indices, byte h, String filePath, long j) {
        try (BufferedWriter writer = new BufferedWriter(new FileWriter(filePath, true))) {
            writer.write("TX_index: " + j);
            writer.newLine();
            for (int i = 0; i < nodeID.size(); i++) {
                writer.write("color" + color[i] + "_" + h + "_" + 2 + ".json" +"; NodeID: " + nodeID.get(color[i]) + "; Index: " + indices.get(color[i]));
                writer.newLine();
            }
            System.out.println("Indexing written to " + filePath);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    //Enter the height of a tree h from keyboard. h has to greater than or equal to 2.
    public static Byte h() {
        byte height;
        Scanner input = new Scanner(System.in);
        do {
            System.out.print("Enter the height of a tree \"h\" = ");
            while (!input.hasNextByte()) {
                System.out.print("NOTE! You have to put integer of the height \"h\" = ");
                input.next();
            }
            height = input.nextByte();
            if (height < 2) {
                System.out.println("NOTE! Enter an Integer >= 2");
            }
        } while (height < 2); //The height "h" input has to greater than or equal 2

        return height;
    }
}


//-----------------------------------------Set of Nodes have the same color class------------------------------------------------------------
/* An example of a perfect binary tree with h = 2
                       1
                    /    \
                  2       3
                /  \    /  \
               4   5   6    7
*/
//Each set will contain all node with the same color
class NodesSet {

    private int count = 0;
    private final char colorSet;
    //Array of node IDs (R)
    private final int [] addNodes;

    public NodesSet(char colorSet, int size) {
        this.colorSet = colorSet;
        addNodes = new int [size];
    }

    public void addNode(int root) {
        //array start from 0, whereas node colored start from 2
        addNodes[count] = root;
        count++;
    }

    public int [] getAddNodes() {
        return addNodes;
    }

    public char getColorSet() {
        return colorSet;
    }

    public int getSize() {
        return count;
    }

    @Override
    public String toString() {
        StringBuilder roots = new StringBuilder();
        for (int addNode : addNodes) {
            roots.append(addNode).append(" ");
        }

        return roots.toString();
    }
}


//-----------------------------------------NumColor class---------------------------------------------------------------
//Stored and Sorted vector c.
class NumColor implements Comparable<NumColor> {

    private final int size;
    private final char color;

    public NumColor(char color, int size) {
        this.color = color;
        this.size = size;
    }

    public char getColor() {
        return color;
    }

    public int getSize() {
        return size;
    }

    @Override
    public int compareTo(NumColor otherNumColor) {
        return Integer.compare(getSize(), otherNumColor.getSize());
    }
}


//---------------------------------------------------------Data export class---------------------------------------------------------------
//Export Output. An example of T(2) as below: (first line is the height of the tree; second line and third line are nodes of the same color
//2
//2 6 7
//3 4 5
class Data {

    private final String PATH;
    private String EXP;

    public Data(String EXP, String PATH) {
        this.PATH = PATH;
        this.EXP = EXP;
    }

    public void SaveTreeTXT(NodesSet[] NodesSets) throws IOException {
        if(this.EXP.contains("ON")) {
            File file = new File(PATH + NodesSets.length + ".txt"); //Create testh.txt
            FileWriter out = new FileWriter(file, false); //Set true for append mode
            PrintWriter output = new PrintWriter(out);

            output.println(NodesSets.length);

            Arrays.stream(NodesSets).forEach(s -> {
                Arrays.stream(s.getAddNodes()).forEach(r -> output.print(r + " "));
                output.println();
            });

            output.close();
        }
    }
}


//-----------------------------------------CHECK the CSA outputs class---------------------------------------------------------------
class CheckCSA{

    //Check if:
    // 1. All nodes in the tree (except root) appear exactly once across S
    // 2. All node IDs in the tree (except root) are inside the range of IDs (from 2 to (2^(h+1) - 2))
    // 3. Total number of nodes in the tree is the same as total nodes in the perfect binary tree.
    public boolean IsValidAllNodeID(NodesSet[] S, int h) {
        int nodeID;
        int numNodes = 0;
        int n = (int) Math.pow(2, h);
        int minNode = 2, maxNode = 2 * n - 1;

        if (S.length !=h ) {
            System.out.println("The size of S is not " + h);
            return false;
        }

        //true if the corresponding node has been allocated to some sets before
        boolean[] isAllocated = new boolean[2 * n];

        for (int i = 0; i < isAllocated.length; i++) {
            isAllocated[i] = false;
        }

        for (NodesSet nodesSet : S) {
            numNodes += nodesSet.getSize();
            for (int j = 0; j < nodesSet.getSize(); j++) {
                nodeID = nodesSet.getAddNodes()[j];
                if ((nodeID < minNode) || (nodeID > maxNode)) {
                    System.out.println("ERROR: Nodes are out of range: " + nodeID);
                    return false;
                }
                if (isAllocated[nodeID]) {
                    System.out.println("ERROR: Overlapping sets");
                    return false;
                }
                isAllocated[nodeID] = true;
            }
        }

        if (numNodes != 2 * n - 2) {
            System.out.println("ERROR: The total number of tree nodes is not correct");
            return false;
        }

        return true;
    }

    //Check if sets have valid sizes
    public boolean IsBalancedSets(NodesSet[] S, int h) {
        Set<Integer> validSizes = setSizes(h);
        //Second, test if sets have valid sizes
        for (int i = 0; i < S.length; i++) {
            if (!validSizes.contains(S[i].getSize())) {
                //System.out.println("ERROR: Set " + i + " has an invalid size = " + S[i].getSize());
                return false;
            }
        }

        return true;
    }

    //Check if no ancestor-descendant pair appears in any set
    public boolean IsRelationship(NodesSet[] S) {
        for (int i = 0; i < S.length; i++) {
            Arrays.sort(S[i].getAddNodes());
            for (int j = 0; j < S[i].getSize(); j++) {
                if (IsDescendantOf(S[i].getAddNodes()[j], S[i])) {
                    System.out.println("ERROR: Set " + i + " contains an ancestor-descendant pair");
                    return false;
                }
            }
        }

        return true;
    }

    //Using the rule: leftchild = 2*parent; rightchild = 2*parent+1
    public static boolean IsDescendantOf(int descendant, int ancestor) {
        int parent = descendant / 2;

        while (parent > ancestor) {
            parent = parent / 2;
        }

        return parent == ancestor;
    }

    public static boolean IsDescendantOf(int descendant, NodesSet ancestorsList) {
        for (int i = 0; i < ancestorsList.getSize(); i++) {
            if (IsDescendantOf(descendant, ancestorsList.getAddNodes()[i])) return true;
        }
        return false;
    }

    public Set<Integer> setSizes(int h) {
        Set<Integer> validSizes = new LinkedHashSet<>(2);
        int n = (int) Math.pow(2, h);
        validSizes.add((2 * n - 2) / h);

        if ((2 * n - 2) % h != 0) {
            validSizes.add((2 * n - 2) / h + 1);
        }

        return validSizes;
    }
}

//-----------------------------------------Find Sub-Indices class---------------------------------------------------------------
class SubIndices {
    private byte h; //h is the height of a tree
    private long j; //a leaf index
    char[] color = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q'
            , 'R','S', 'T', 'U', 'V','W', 'X', 'Y', 'Z','0', '1', '2', '3', '4', '5', '6', '7','8', '9'};

    HashMap<List<NumColor>, List<NumColor>> C = new HashMap<>();

    public SubIndices(byte h, long j) {
        this.h = h;
        this.j = j;
    }

    //Finding the path from root to leaf
    public int[] Path() {
        int[] path = new int[h];
        long[] path_nodeID = new long[h];
        int temp;

        temp = (int) (j % 2);

        if (temp == 0) {
            path[h - 1] = 2;
        }
        else {
            path[h - 1] = temp;
        }

        path_nodeID[h - 1] = first_nodeID(h) + j - 1;
        //Node ID from bottom to top
        for (byte i = (byte) (h - 2); i >= 0; i--) {
            path_nodeID[i] = (long) Math.ceil((double)(path_nodeID[i + 1] - 1)/2);
        }
        //Path position from top to bottom
        for (byte i = 0; i < h ; i++) {
            temp = (int) (((path_nodeID[i] - first_nodeID((byte) (i + 1)) + 1)) % 2);
            if (temp == 0) {
                path[i] = 2;
            }
            else {
                path[i] = temp;
            }
        }

        return path;
    }

    //Finding the path node IDs from root to leaf
    public long[] PathID() {
        long[] path_nodeID = new long[h];

        path_nodeID[h - 1] = first_nodeID(h) + j - 1;
        //Node ID from bottom to top
        for (byte i = (byte) (h - 2); i >= 0; i--) {
            path_nodeID[i] = (long) Math.ceil((double)(path_nodeID[i + 1] - 1)/2);
        }
        return path_nodeID;
    }

    //
    public int[] Indices(int[] path, List<NumColor> c) {
        int[] count = new int[h];
        int[] indices = new int[h];
        int x = 2;
        byte height;
        char root_col;

        for (byte l = 0; l < h - 1; l++) {
            height = (byte) (h - l);
            List<NumColor> a = new ArrayList<>(height - 1);
            List<NumColor> b = new ArrayList<>(height - 1);

            if (height > 0) {
                //Assign Color 1 to all children;
                if (c.get(0).getSize() == 2) {
                    x = 2;
                }
                else {
                    x = 1;
                }

                //Split the feasible sequence c to q feasible sequences for q sub-trees.
                if (height > 1) {
                    //If the recent root node color by the first or second color of the recent sequence c, the below its color will be 0
                    if (path[l] <= x) {
                        root_col = c.get(0).getColor();
                        count[find_color(c.get(0).getColor())] += path[l] - 1;
                        count[find_color(c.get(1).getColor())] += 2 - x;
                    }
                    else {
                        root_col = c.get(1).getColor();
                        count[find_color(c.get(1).getColor())] += path[l] - x - 1;
                        count[find_color(c.get(0).getColor())] += x;
                    }

                    C = FeasibleSplit(height, c);
                    C.forEach((n, m) -> {
                        a.addAll(n);
                        b.addAll(m);
                    });

                    if (path[l] == 2) {
                        for (int i = 0; i < height - 1; i++) {
                            if (root_col != a.get(i).getColor())
                                count[find_color(a.get(i).getColor())] += a.get(i).getSize();
                        }
                    }

                    if ( path[l] == 1) {
                        c = a;
                    }
                    else {
                        c = b;
                    }

                    if (c.size() == 1) {
                        count[find_color(c.get(0).getColor())] += path[path.length - 1] - 1;
                    }
                    else
                        Collections.sort(c);
                }
            }
        }

        /*for (int i = 0; i < h; i++) {
            indices[i] = count[i] + 1;
        }*/

        return count;
    }

    public LinkedHashMap<Character, Integer> mapIndices(int[] path, List<NumColor> c) {
        LinkedHashMap<Character, Integer> map = new LinkedHashMap<>();
        int[] count = new int[h];
        int x;
        byte height;
        char root_col;

        for (byte l = 0; l < h - 1; l++) {
            height = (byte) (h - l);
            List<NumColor> a = new ArrayList<>(height - 1);
            List<NumColor> b = new ArrayList<>(height - 1);

            if (height > 0) {
                //Assign Color 1 to all children;
                if (c.get(0).getSize() == 2) {
                    x = 2;
                }
                else {
                    x = 1;
                }

                //Split the feasible sequence c to q feasible sequences for q sub-trees.
                if (height > 1) {
                    //If the recent root node color by the first or second color of the recent sequence c, the below its color will be 0
                    if (path[l] <= x) {
                        root_col = c.get(0).getColor();
                        count[find_color(c.get(0).getColor())] += path[l] - 1;
                        count[find_color(c.get(1).getColor())] += 2 - x;
                        map.put(c.get(0).getColor(), count[find_color(c.get(0).getColor())]);
                    }
                    else {
                        root_col = c.get(1).getColor();
                        count[find_color(c.get(1).getColor())] += path[l] - x - 1;
                        count[find_color(c.get(0).getColor())] += x;
                        map.put(c.get(1).getColor(), count[find_color(c.get(1).getColor())]);
                    }

                    C = FeasibleSplit(height, c);
                    C.forEach((n, m) -> {
                        a.addAll(n);
                        b.addAll(m);
                    });

                    if (path[l] == 2) {
                        for (int i = 0; i < height - 1; i++) {
                            if (root_col != a.get(i).getColor())
                                count[find_color(a.get(i).getColor())] += a.get(i).getSize();
                        }
                    }

                    if ( path[l] == 1) {
                        c = a;
                    }
                    else {
                        c = b;
                    }

                    if (c.size() == 1) {
                        count[find_color(c.get(0).getColor())] += path[path.length - 1] - 1;
                        map.put(c.get(0).getColor(), count[find_color(c.get(0).getColor())]);
                    }
                    else
                        Collections.sort(c);
                }
            }
        }

        return map;
    }

    //Finding first node ID in a specific layer
    public long first_nodeID(byte l) {
        return (int) (Math.pow(2, l) - 1) + 1;
    }

    //Finding final node ID in a specific layer
    public long final_nodeID(byte l) {
        return (int) (Math.pow(2, l + 1) - 1);
    }

    //Finding position of a character in a character array.
    public int find_color(char target){
        int pos = -1;
        char[] color = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q'
                , 'R','S', 'T', 'U', 'V','W', 'X', 'Y', 'Z','0', '1', '2', '3', '4', '5', '6', '7','8', '9'};

        for (int i = 0; i < color.length; i++) {
            if (color[i] == target) {
                pos = i;
                break; // Exit loop once the target is found
            }
        }
        return pos;
    }

    /* This algorithm splits a ℎ-feasible sequence into two (ℎ − 1)-feasible ones, which will be used for coloring the subtrees; only works when ℎ ≥ 2.
    Note that the splitting rule (see FeasibleSplit(ℎ, 𝑐)) ensures that if Color 𝑖 is used for a node then it will no longer be used in the subtree rooted at that node,
    hence guaranteeing the Ancestral Property.*/
    //key = a; value = b which are two (ℎ − 1)-feasible of two subtrees following "Procedure FeasibleSplit(ℎ, 𝑐)"
    public HashMap<List<NumColor>, List<NumColor>> FeasibleSplit(byte h, List<NumColor> c) {
        List<NumColor> a = new ArrayList<>(h - 1);
        List<NumColor> b = new ArrayList<>(h - 1);
        C.clear();

        if (h == 2) {
            byte i = 1; //Position 2
            int aValue, bValue;

            if (c.get(0).getSize() == 2) {
                aValue = c.get(i).getSize() / 2;
                bValue = c.get(i).getSize() / 2;
                a.add(new NumColor(c.get(1).getColor(), aValue));
                b.add(new NumColor(c.get(1).getColor(), bValue));
            }
            else {
                aValue = c.get(i).getSize() - 1;
                bValue = c.get(0).getSize() - 1;
                a.add(new NumColor(c.get(1).getColor(), aValue));
                b.add(new NumColor(c.get(0).getColor(), bValue));
            }

            Collections.sort(a);
            Collections.sort(b);
            C.put(a, b);

            return C;
        }
        else if (h > 2) {
            //Case 1: 𝑐1 = 2
            if (c.get(0).getSize() == 2) {
                byte i = 1; //Position 2
                int aValue = (int) Math.floor(c.get(i).getSize() / 2.0);
                int bValue = (int) Math.ceil(c.get(i).getSize() / 2.0);
                a.add(new NumColor(c.get(i).getColor(), aValue));
                b.add(new NumColor(c.get(i).getColor(), bValue));
                int Sa = aValue; //Left sum
                int Sb = bValue; //Right sum
                i++;
                //change for original algorithm
                while (i < h) {
                    if (Sa < Sb) {
                        aValue = (int) Math.ceil(c.get(i).getSize() / 2.0);
                        bValue = (int) Math.floor(c.get(i).getSize() / 2.0);
                    }
                    else {
                        aValue = (int) Math.floor(c.get(i).getSize() / 2.0);
                        bValue = (int) Math.ceil(c.get(i).getSize() / 2.0);
                    }
                    a.add(new NumColor(c.get(i).getColor(), aValue));
                    b.add(new NumColor(c.get(i).getColor(), bValue));
                    Sa += aValue;
                    Sb += bValue;
                    i++;
                }
            }
            //Case 2: 𝑐1 > 2; note that 𝑐1 ≥ 2 due to the feasibility of 𝑐;
            else {
                byte i = 1; //Position 2
                int aValue = c.get(i).getSize() - 1;
                int bValue = c.get(0).getSize() - 1;
                a.add(new NumColor(c.get(1).getColor(), aValue));
                b.add(new NumColor(c.get(0).getColor(), bValue));
                int Sa = aValue; //Left sum
                int Sb = bValue; //Right sum
                i++;
                aValue = (int) Math.ceil((c.get(i).getSize() + c.get(0).getSize() - c.get(i - 1).getSize()) / 2.0);
                bValue = c.get(i - 1).getSize() - c.get(0).getSize() + (int) Math.floor((c.get(i).getSize() + c.get(0).getSize() - c.get(i - 1).getSize()) / 2.0);
                a.add(new NumColor(c.get(i).getColor(), aValue));
                b.add(new NumColor(c.get(i).getColor(), bValue));
                Sa += aValue;
                Sb += bValue;
                i++;
                while (i < h) {
                    if (Sa < Sb) {
                        aValue = (int) Math.ceil(c.get(i).getSize() / 2.0);
                        bValue = (int) Math.floor(c.get(i).getSize() / 2.0);
                    }
                    else {
                        aValue = (int) Math.floor(c.get(i).getSize() / 2.0);
                        bValue = (int) Math.ceil(c.get(i).getSize() / 2.0);
                    }
                    a.add(new NumColor(c.get(i).getColor(), aValue));
                    b.add(new NumColor(c.get(i).getColor(), bValue));

                    Sa += aValue;
                    Sb += bValue;
                    i++;
                }
            }
            Collections.sort(a);
            Collections.sort(b);
            C.put(a, b);
            return C;
        }
        else {
            System.out.println("Warming! h should be greater than or equal 2"); //𝑐 ≥ 2 due to the feasibility of 𝑐;
            return null;
        }
    }

    //Corollary 2.12 (Balanced Color sequence) return 𝑐 = [𝑐1, 𝑐2, . . . , 𝑐ℎ]
    public List<NumColor> balancedColorSequence(byte h) {
        int cValue;
        byte u = (byte) ((Math.pow(2, h + 1) - 2) % h);
        List<NumColor> c = new ArrayList<>(h);

        for (byte i = 0; i < h; i++) {
            double a = (Math.pow(2, h + 1) - 2) / h;
            if (i < (h - u)) {
                cValue = (int) Math.floor(a);
                c.add(new NumColor(color[i], cValue));
            }
            else {
                cValue = (int) Math.ceil(a);
                c.add(new NumColor(color[i], cValue));
            }
        }

        return c;
    }
}
