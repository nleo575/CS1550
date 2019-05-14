import java.io.*;
import java.util.*;
/**
    @author Nicolas Leo
    CS 1550 Project 3
    
    Virtual Memory Simulator
    This program simulates 4 VM page replacement algorithms: 
        Optimal
        Clock
        First-In First-Out (FIFO)
        Not Recently Used (NRU)
    execution
    java vmsim –n <numframes> -a <opt|clock|fifo|nru> [-r <refresh>] <tracefile>
 */
public class vmsim 
{
    private static int maccesses = 0, // Number of memory accesses
                       faults    = 0, // Number of page faults
                       writes    = 0; // Number of dirty pages written to disk
    private static HashMap<Long, Node> PT;  // PT used int optimal algorithm
    private static Node hand, // Used for the clock algorithm
                        head, tail; // Used for linked list algorithms
    

    public static void main(String[] args)
    {
        String tracefile = null;
        int refresh = 0, frames = 0;
        if (args.length == 5)
        {
            frames = Integer.parseInt(args[1]);
            tracefile = args[4];
        } 
        else if (args.length == 7)
        {
            frames = Integer.parseInt(args[1]);
            refresh = Integer.parseInt(args[5]);
            tracefile = args[6];
        } 
        else 
        {
            System.out.println("Invalid number of arguments");
            System.exit(0);
        }

        if (args[3].equalsIgnoreCase("opt")) opt(tracefile, frames);
        else if (args[3].equalsIgnoreCase("clock")) clock(tracefile, frames);
        else if (args[3].equalsIgnoreCase("fifo")) fifo(tracefile, frames);  
        else if (args[3].equalsIgnoreCase("nru")) nru(tracefile, frames,refresh);
        else
        {
            System.out.println("Invalid algorithm. Exiting.");
            System.exit(0);
        }

        // Print the results
        System.out.printf("\nAlgorithm: %s\n", args[3].toUpperCase());
        if(args.length == 7) 
            System.out.printf("Refresh rate:     \t%,9d\n", refresh);
        System.out.printf("Number of frames: \t%,9d\n", frames);
        System.out.printf("Total memory accesses: \t%,9d\n", maccesses);
        System.out.printf("Total page faults: \t%,9d\n", faults);
        System.out.printf("Total writes to disk:\t%,9d\n", writes);
        System.exit(0);
    }
    /**
        Check the page table (PT) hash map for the given page. If found update referenced &
        dirty (if necessary).
        @param hexString Raw hexadecimal string read from the input file
        @param page Page number
        @param access Page access ('W' or 'R')
        @return True if the given page is in the PT. False otherwise
    */
    private static boolean hasOptPage(long address, boolean dirty)
    {
        // Check the PT (hash map) for the given key (page number)
        Node temp;
        if((temp = PT.get(new Long(address >> 12))) != null)
        {
            System.out.printf("%#010x - HIT\n", address);
            temp.referenced = true;
            if(dirty) temp.dirty = true;
            return true;
        } 

        return false;
    }
    /**
        Optimal page replacement algorithm. On fault, evicts the page that will be used
        farthest in the future (if it is used at all). 
        @param fileName String of the trace file's name.
        @param frames Number of frames in the PT
    */
    private static void opt(String fileName, int frames)
    {
        char access;
        int  biggestAccess, listLen;
        long page; 
        LinkedList<Node> list;
        Node node = null, farthestN = null;
        PT = new HashMap<Long, Node>();
        HashMap<Long, LinkedList<Node>> futures = new HashMap<Long, LinkedList<Node>>();

        // head & tail defined statically and used only for reading ahead in the file, not 
        // for the PT
        Scanner sc = null;
        try {sc = new Scanner(new File(fileName));}
        catch (Exception e)
        {
            System.out.println("Error opening file. Exiting.");
            System.exit(0);
        }
        
        String  hexString;   

        // Pre-process file
        if(sc.hasNextLong(16))
        {
            hexString = sc.next(); 
            page = Long.parseLong(hexString, 16);
            access = sc.next().charAt(0);
            head = new Node(page, access, maccesses);
            maccesses++;

            list = new LinkedList<Node>();
            list.add(node);
            futures.put(new Long(page>>12),list); 
        }
        else return;  

        node = head;
        while(sc.hasNextLong(16)) 
        {
            hexString = sc.next(); 
            page = Long.parseLong(hexString, 16);
            access = sc.next().charAt(0);
            node.next = new Node(page, access, maccesses);
            node = node.next;
            maccesses++;

            if((list = futures.get(new Long(page>>12))) != null) list.add(node); 
            else 
            {
                list = new LinkedList<Node>();
                list.add(node);
                futures.put(new Long(page>>12),list); // Add page to the PT
            } 
        }

        // Adding first page to PT. There's a fault
        System.out.printf("%#010x - FAULT - no eviction\n", head.page);
        listLen = faults = 1;
        
        PT.put(new Long(head.page >> 12), node); // Add page to PT
        list = futures.get(head.page >> 12); // Remove entry from LL for this page
        if(!list.isEmpty()) list.pop();
        head = head.next;

        while(head!=null && listLen < frames)
        {
            if(hasOptPage(head.page, head.dirty))
            { 
                list = futures.get(head.page >> 12);
                if(!list.isEmpty()) list.pop();// Remove entry from LL for this page
                head = head.next;
                continue;
            }

            System.out.printf("%#010x - FAULT - no eviction\n", head.page);
            faults++; listLen++;

            PT.put(new Long(head.page >> 12), head);// Add page to PT
            list = futures.get(head.page >> 12); // Remove entry from LL for this page
            if(!list.isEmpty()) list.pop();
            head = head.next;
        }   

        while (head!=null)
        {
            // Read next address if page was updated.
            if(hasOptPage(head.page, head.dirty))
            {
                list = futures.get(head.page >> 12);
                if(!list.isEmpty()) list.pop();// Remove entry from LL for this page
                head = head.next;
            } 
            else // Page wasn't found. FAULT
            {
                // Evict the page that is used needed farthest in the future
                faults++; 
                biggestAccess = 0;

                // Check each PTE for the one that's used farthest in the future
                for (Map.Entry<Long, Node> entry : PT.entrySet()) 
                {
                    if((list = futures.get(entry.getKey()))!= null)
                    {
                        if((node = list.peek()) != null) 
                        {    
                            if(node.readNum > biggestAccess)
                            { 
                                biggestAccess = node.readNum; 
                                farthestN = node; 
                            }
                        }
                        else
                        {
                            farthestN = entry.getValue(); break;
                        }
                    }
                    else // If list isn't in futures this page isn't used again
                    {
                        farthestN = entry.getValue(); break;
                    }
                } 

                // Evict the farthest used
                System.out.printf("%#010x - FAULT - evict %s (%#x)\n",
                    head.page, farthestN.dirty ?"dirty": "clean", 
                    farthestN.page >> 12 );
                if(farthestN.dirty) writes++;

                PT.remove(new Long(farthestN.page >> 12));
                PT.put(new Long(head.page >> 12), head);

                list = futures.get(head.page >> 12);
                if(!list.isEmpty()) list.pop();// Remove entry from LL for this page
                head = head.next;
            }
        }   
    }

    /**
        Check the CIRCULAR page table (PT) for the given page. If found update referenced &
        dirty (if necessary).
        @param length Length of the linked list
        @param hexString Raw hexadecimal string read from the input file
        @param page Page number
        @param access Page access ('W' or 'R')
        @return True if the given page is in the PT. False otherwise
    */
    private static boolean hasCircularPage(int length, String hexString, long page, 
        char access)
     {
        Node temp = hand;
        for (int i = 0; i < length ; i++) 
        {
            if (temp.page == page)
            {
                System.out.printf("0x%s - HIT\n", hexString);
                temp.referenced = true;
                if(access == 'W') temp.dirty = true;
                return true;  
            }
            temp = temp.next;
        }
        return false;
    }

    /**
        Clock algorithm. On fault, evicts the next unreferenced page. 
        @param fileName String of the trace file's name.
        @param frames Number of frames in the PT
    */
    private static void clock(String fileName, int frames)
    {
        char    access;
        int     listLen = 0;
        long    page; 

        Scanner sc = null;
        try {sc = new Scanner(new File(fileName));}
        catch (Exception e)
        {
            System.out.println("Error opening file. Exiting.");
            System.exit(0);
        }
        String  hexString;   

        if(sc.hasNextLong(16))
        {
            // Read the 1st address & access from the file
            hexString = sc.next(); 
            page = Long.parseLong(hexString, 16);
            page >>= 12;
            access = sc.next().charAt(0);
           
            System.out.printf("0x%s: FAULT - no eviction\n", hexString);

            maccesses = listLen = faults = 1;
            head = new Node(page, access); // Store page # & access

            hand = tail = head; head.next = head;
        }
        else return;   

        // PT is not full. 
        while(sc.hasNextLong(16) && listLen < frames)
        {
            maccesses++;
            hexString = sc.next(); 
            page = Long.parseLong(hexString, 16);
            page >>= 12;
            access = sc.next().charAt(0);


            if(hasCircularPage(listLen, hexString, page, access)) continue;

            System.out.printf("0x%s: FAULT - no eviction\n", hexString);
            faults++; listLen++;

            // Add page to the PT
            tail.next = new Node(page, access);
            tail = tail.next; tail.next = head;
        }   

        while (sc.hasNextLong(16))
        {
            maccesses++;
            hexString = sc.next(); 
            page = Long.parseLong(hexString, 16);
            page >>= 12;
            access = sc.next().charAt(0);

            // Read next address if page was updated.
            if(hasCircularPage(listLen, hexString, page, access)) {}
            else // Page wasn't found. FAULT
            {
                faults++; 
                while (true)
                {
                    // Look for the next unreferenced page, marking referenced pages as
                    // unreferenced until one is found. 
                    if (!hand.referenced)
                    {
                        System.out.printf("0x%s: FAULT - evict %s (%#x)\n", hexString,
                            hand.dirty ?"dirty": "clean", hand.page);
                        if(hand.dirty) writes++;
                        hand.page = page;
                        hand.referenced = true;
                        hand.dirty = access == 'W'? true: false; 
                        break; 
                    } 
                    else 
                    {
                        hand.referenced = false; hand = hand.next;
                    }
                }
            }
        }   
    }

    /**
        First-in-first-out (FIFO) algorithm. On fault, evicts the oldest entry in the PT.
        @param fileName String of the trace file's name.
        @param frames Number of frames in the PT
    */
    private static void fifo(String fileName, int frames)
    {
        char    access;
        int     listLen;
        long    page; 

        Scanner sc = null;
        try {sc = new Scanner(new File(fileName));}
        catch (Exception e)
        {
            System.out.println("Error opening file. Exiting.");
            System.exit(0);
        }
        String  hexString;        

        if(sc.hasNextLong(16))
        {
            // Read the 1st address & access from the file
            hexString = sc.next(); 
            page = Long.parseLong(hexString, 16);
            page >>= 12;
            access = sc.next().charAt(0);
           
            System.out.printf("0x%s: FAULT - no eviction\n", hexString);

            maccesses = listLen = faults = 1;
            head = new Node(page, access); // Store page # & access

            tail = head;
        }
        else return;   

        // PT is not full. 
        while(sc.hasNextLong(16) && listLen < frames)
        {
            maccesses++;
            hexString = sc.next(); 
            page = Long.parseLong(hexString, 16);
            page >>= 12;
            access = sc.next().charAt(0);

            if(hasPage(hexString, page, access)) continue;

            System.out.printf("0x%s: FAULT - no eviction\n", hexString);
            faults++; listLen++;

            // Add page to the PT
            tail.next = new Node(page, access);
            tail = tail.next;
        }     

        while (sc.hasNextLong(16)) // read until the EOF
        {
            maccesses++;

            // Read the next address & access from the file
            hexString = sc.next(); 
            page = Long.parseLong(hexString, 16);
            page >>= 12;
            access = sc.next().charAt(0);

            // Read next address if page was updated.
            if(hasPage(hexString, page, access))  {} 
            else // Page wasn't found. FAULT
            {
                faults++;
                // Evict the oldest PTE. 
                System.out.printf("0x%s - FAULT - evict %s (%#x)\n", hexString,
                            head.dirty ?"dirty": "clean", head.page);
                if(head.dirty) writes++;
                tail.next = head; tail = head; head = head.next; tail.next = null; 
                tail.page = page; tail.referenced = true;
                tail.dirty = access == 'W'? true: false; 
            }
        }
    }

    /**
        Not-Recently-Used (NRU) algorithm. Evicts lowest based on classifications 0-3.
        Class 0: not referenced, clean
        Class 1: not referenced, dirty
        Class 2: referenced, clean
        Class 3: referenced, dirty
        Also periodically sets all PTE's referenced bit to false;
        @param fileName String of the trace file's name.
        @param frames Number of frames in the PT
        @param refresh How many cycles to wait before resetting the referenced bits to false
    */
    private static void nru(String fileName, int frames, int refresh)
    {
        char access;
        int  classification = 0, listLen = 0, lowestFound, reads = 0;
        long page; 
        
        Node    temp, lowest = null, resume = null; // head & tail defined statically
        Scanner sc = null;
        try {sc = new Scanner(new File(fileName));}
        catch (Exception e)
        {
            System.out.println("Error opening file. Exiting.");
            System.exit(0);
        }
        String  hexString;
        
        if(sc.hasNextLong(16))
        {
            // Read the 1st address & access from the file
            hexString = sc.next(); 
            page = Long.parseLong(hexString, 16);
            page >>= 12;
            access = sc.next().charAt(0);
           
            System.out.printf("0x%s - FAULT - no eviction\n", hexString);

            reads = maccesses = listLen = faults = 1;
            head = new Node(page, access); // Store page # & access

            tail = head;
        }
        else return;

        // PT is not full. 
        while(sc.hasNextLong(16) && listLen < frames)
        {
            if (reads >= refresh) // Set all referenced bits to false & reset read count
            {
                reads = 0;
                temp = head;
                while (temp != null)
                {
                    temp.referenced = false;
                    temp = temp.next;
                }
            }

            reads++; maccesses++;
            hexString = sc.next(); 
            page = Long.parseLong(hexString, 16);
            page >>= 12;
            access = sc.next().charAt(0);

            if(hasPage(hexString, page, access)) continue;

            System.out.printf("0x%s - FAULT - no eviction\n", hexString);
            faults++; listLen++;

            // Add page to the PT
            tail.next = new Node(page, access);
            tail = tail.next;
        }    

        resume = tail;

        while (sc.hasNextLong(16)) // PT is full. Read until the EOF
        {
            if (reads >= refresh) // Set all referenced bits to false & reset read count
            {
                reads = 0;
                temp = head;
                while (temp != null)
                {
                    temp.referenced = false;
                    temp = temp.next;
                }
            }

            // Read the next address & access from the file
            reads++; maccesses++;
            hexString = sc.next(); 
            page = Long.parseLong(hexString, 16);
            page >>= 12;
            access = sc.next().charAt(0);


            // Check the PT for page, if found update referenced & dirty (if necessary)
            if(hasPage(hexString, page, access)) {}
            else // Page wasn't found. FAULT
            {
                faults++;
                temp = resume.next == null? head: resume.next;
                lowest = temp; // Keep track of the lowest PTE in order to evict
                lowestFound = 4; 

                // Search PT in circular manner to avoid leaving some entries untouched
                // Evict the lowest class. Immediately evict class 0.
                while (true)
                {
                    // Classify the current page
                    if (!temp.referenced && !temp.dirty) // Class 0, immediate eviction
                    {
                        System.out.printf("0x%s - FAULT - evict clean (%#x)\n", 
                            hexString, temp.page);
                        // Replace current page with new page contents. 
                        temp.page = page;
                        temp.dirty = access == 'W'? true: false; 
                        temp.referenced = true;
                        resume = temp; // Start the search next time from here
                        break;
                    }                   

                    else if (!temp.referenced && temp.dirty) classification = 1;
                    else if (temp.referenced && !temp.dirty) classification = 2;
                    else if (temp.referenced && temp.dirty) classification = 3;

                    if(classification <= lowestFound) 
                    {
                        lowestFound = classification; 
                        lowest = temp; 
                    }

                    if (temp == resume) 
                    {   // Reached the last entry in the PT, evict the lowest
                        System.out.printf("0x%s - FAULT - evict %s (%#x)\n", hexString,
                            lowest.dirty ?"dirty": "clean", lowest.page);
                        // Replace current page with new page contents. 
                        if(lowest.dirty) writes++;
                        lowest.page = page;
                        lowest.dirty = access == 'W'? true: false; 
                        lowest.referenced = true;
                        resume = lowest;
                        break;
                    }
                    else temp = temp.next == null? head: temp.next; 
                }
            }
        }
    }

    /**
        Check the page table (PT) for the given page. If found update referenced & 
        dirty (if necessary).
        @param hexString Raw hexadecimal string read from the input file
        @param page Page number
        @param access Page access ('W' or 'R')
        @return True if the given page is in the PT. False otherwise
    */
    private static boolean hasPage(String hexString, long page, char access)
    {
        Node temp = head;
        while (temp != null)
        {
            if (temp.page == page)
            {
                System.out.printf("0x%s - HIT\n", hexString);
                temp.referenced = true;
                if(access == 'W') temp.dirty = true; 
                return true;  
            }
            temp = temp.next;
        }
        return false;
    }

    // Node class used for linked lists in NRU & clock. 
    private static class Node 
    {
        int readNum; // Used in Optimal algorithm to keep track when this page is seen
        long page;
        boolean dirty, referenced = true;
        Node next   = null;   // Used for traversing as a normal linked list

        private Node(long p, char a)
        {
            page = p;
            dirty = a == 'W'? true: false;
        }
        private Node(long p, char a, int r)
        {
            page = p;
            dirty = a == 'W'? true: false;
            readNum = r; 
        }
    }
}