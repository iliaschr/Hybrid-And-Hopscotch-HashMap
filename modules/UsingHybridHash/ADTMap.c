/////////////////////////////////////////////////////////////////////////////
//
// Υλοποίηση του ADT Map μέσω υβριδικού Hash Table
//
/////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>
#include "ADTMap.h"

#include "ADTVector.h" //Για το vector που θα χρησιμοποιήσουμε για το chaining

// Οι κόμβοι του map στην υλοποίηση με hash table, μπορούν να είναι σε 3 διαφορετικές καταστάσεις,
// ώστε αν διαγράψουμε κάποιον κόμβο, αυτός να μην είναι empty, ώστε να μην επηρεάζεται η αναζήτηση
// αλλά ούτε occupied, ώστε η εισαγωγή να μπορεί να το κάνει overwrite.
typedef enum {
	EMPTY, OCCUPIED
} State;

// Το μέγεθος του Hash Table ιδανικά θέλουμε να είναι πρώτος αριθμός σύμφωνα με την θεωρία.
// Η παρακάτω λίστα περιέχει πρώτους οι οποίοι έχουν αποδεδιγμένα καλή συμπεριφορά ως μεγέθη.
// Κάθε re-hash θα γίνεται βάσει αυτής της λίστας. Αν χρειάζονται παραπάνω απο 1610612741 στοχεία, τότε σε καθε rehash διπλασιάζουμε το μέγεθος.
int prime_sizes[] = {53, 97, 193, 389, 769, 1543, 3079, 6151, 12289, 24593, 49157, 98317, 196613, 393241,
	786433, 1572869, 3145739, 6291469, 12582917, 25165843, 50331653, 100663319, 201326611, 402653189, 805306457, 1610612741};

// Χρησιμοποιούμε open addressing, οπότε σύμφωνα με την θεωρία, πρέπει πάντα να διατηρούμε
// τον load factor του  hash table μικρότερο ή ίσο του 0.5, για να έχουμε αποδoτικές πράξεις
#define MAX_LOAD_FACTOR 0.5

// Κάθε θέση i θεωρείται γεινοτική με όλες τις θέσεις μέχρι και την i + NEIGHBOURS
#define NEIGHBOURS 3


// Δομή του κάθε κόμβου που έχει το hash table (με το οποίο υλοιποιούμε το map)
struct map_node{
	Pointer key;
	Pointer value;
	State state;
	bool flag;
};

// Δομή του Map (περιέχει όλες τις πληροφορίες που χρεαζόμαστε για το HashTable)
struct map {
	MapNode array;
	int capacity;
	int size;
	CompareFunc compare;
	HashFunc hash_function;
	DestroyFunc destroy_key;
	DestroyFunc destroy_value;
	Vector chaining;
};


Map map_create(CompareFunc compare, DestroyFunc destroy_key, DestroyFunc destroy_value) {
	// Δεσμεύουμε κατάλληλα τον χώρο που χρειαζόμαστε για το hash table
	Map map = malloc(sizeof(*map));
	map->capacity = prime_sizes[0];
	map->array = malloc(map->capacity * sizeof(struct map_node));
	
	map->chaining = vector_create(map->capacity, NULL); //φτιαχνουμε ενα vector για το seperate chaining

	// Αρχικοποιούμε τους κόμβους που έχουμε σαν διαθέσιμους.
	for (int i = 0; i < map->capacity; i++){
		map->array[i].state = EMPTY;
		vector_set_at(map->chaining, i, NULL); //αρχικοποιουμε το vector
	}
		
	map->size = 0;
	map->compare = compare;
	map->destroy_key = destroy_key;
	map->destroy_value = destroy_value;

	return map;
}

// Επιστρέφει τον αριθμό των entries του map σε μία χρονική στιγμή.
int map_size(Map map) {
	return map->size;
}

static void rehash(Map map) {
	// Αποθήκευση των παλιών δεδομένων
	int old_capacity = map->capacity;
	MapNode old_array = map->array;

	// Βρίσκουμε τη νέα χωρητικότητα, διασχίζοντας τη λίστα των πρώτων ώστε να βρούμε τον επόμενο. 
	int prime_no = sizeof(prime_sizes) / sizeof(int);	// το μέγεθος του πίνακα
	for (int i = 0; i < prime_no; i++) {					// LCOV_EXCL_LINE
		if (prime_sizes[i] > old_capacity) {
			map->capacity = prime_sizes[i]; 
			break;
		}
	}
	// Αν έχουμε εξαντλήσει όλους τους πρώτους, διπλασιάζουμε
	if (map->capacity == old_capacity)					// LCOV_EXCL_LINE
		map->capacity *= 2;								// LCOV_EXCL_LINE

	// Δημιουργούμε ένα μεγαλύτερο hash table
	map->array = malloc(map->capacity * sizeof(struct map_node));
	Vector temp_chaining = map->chaining; //Pointer για να το εχουμε αποθηκευμενο
	map->chaining = vector_create(map->capacity, NULL); //Ξαναφτιάχνουμε το vector
	for (int i = 0; i < map->capacity; i++){
		map->array[i].state = EMPTY;
		vector_set_at(map->chaining, i, NULL);		   //Αρχικοποιούμε με NULL
	}

	// Τοποθετούμε ΜΟΝΟ τα entries που όντως περιέχουν ένα στοιχείο (το rehash είναι και μία ευκαιρία να ξεφορτωθούμε τα deleted nodes)
	map->size = 0;
	for (int i = 0; i < old_capacity; i++){
		
		if (old_array[i].state == OCCUPIED)
			map_insert(map, old_array[i].key, old_array[i].value);

		if (vector_get_at(temp_chaining, i) != NULL) {
    		Vector chain_vector = vector_get_at(temp_chaining, i);
    		for (VectorNode node = vector_first(chain_vector);
         	node != VECTOR_EOF;
         	node = vector_next(chain_vector, node)) {
        		MapNode temp = vector_node_value(chain_vector, node);
        		if (temp->state == OCCUPIED)
					map_insert(map, temp->key, temp->value);
				free(temp);
    		}
    		vector_destroy(chain_vector);
		}

	}
	
	vector_destroy(temp_chaining);

	//Αποδεσμεύουμε τον παλιό πίνακα ώστε να μήν έχουμε leaks
	free(old_array);
}

// Εισαγωγή στο hash table του ζευγαριού (key, item). Αν το key υπάρχει,
// ανανέωση του με ένα νέο value, και η συνάρτηση επιστρέφει true.

void map_insert(Map map, Pointer key, Pointer value) {
    MapNode node = map_find_node(map, key); // Σκανάρουμε το Hash Table
    if (node != MAP_EOF) {
        if (node->key != key && map->destroy_key != NULL)
            map->destroy_key(node->key);
        if (node->value != value && map->destroy_value != NULL)
            map->destroy_value(node->value);

        node->key = key;
        node->value = value;
        return;
    }

	node = NULL;

    uint pos;
    int count = 0; //μετρητής NEIGHBOURS

    for (pos = map->hash_function(key) % map->capacity; // ξεκινώντας από τη θέση που κάνει hash το key
        map->array[pos].state != EMPTY;					// αν φτάσουμε σε EMPTY σταματάμε
        pos = (pos + 1) % map->capacity, count++) {		// linear probing, γυρνώντας στην αρχή όταν φτάσουμε στη τέλος του πίνακα, αυξάνοντας το count
		if(count == NEIGHBOURS) {
			uint pos = map->hash_function(key) % map->capacity;
			MapNode node = malloc(sizeof(struct map_node));
			node->state = OCCUPIED;
			node->key = key;
			node->value = value;
			node->flag = true;
			if (vector_get_at(map->chaining, pos) == NULL) {
				Vector temp = vector_create(0, NULL);
				vector_set_at(map->chaining, pos, temp);
			}
			vector_insert_last(vector_get_at(map->chaining, pos), node);
			break;
		}
    }

	if (map->array[pos].state == EMPTY) 
		node = &map->array[pos];
	
    if (node != NULL) {
        node->flag = false;
		node->state = OCCUPIED;
		node->key = key;
		node->value = value;
	}

	map->size++; // Νέο στοιχείο, αυξάνουμε τα συνολικά στοιχεία του map

	// Αν με την νέα εισαγωγή ξεπερνάμε το μέγιστο load factor, πρέπει να κάνουμε rehash.
	// Στο load factor μετράμε και τα DELETED, γιατί και αυτά επηρρεάζουν τις αναζητήσεις.
    float load_factor = (float)map->size / map->capacity;
    if (load_factor > MAX_LOAD_FACTOR)
        rehash(map);
}

// Διαργραφή απο το Hash Table του κλειδιού με τιμή key
bool map_remove(Map map, Pointer key) {
	MapNode node = map_find_node(map, key);
	if (node == MAP_EOF)
		return false;

	// destroy
	if (map->destroy_key != NULL)
		map->destroy_key(node->key);
	if (map->destroy_value != NULL)
		map->destroy_value(node->value);

	node->state = EMPTY;
	map->size--;
	return true;
}

// Αναζήτηση στο map, με σκοπό να επιστραφεί το value του κλειδιού που περνάμε σαν όρισμα.

Pointer map_find(Map map, Pointer key) {
	MapNode node = map_find_node(map, key);
	if (node != MAP_EOF)
		return node->value;
	else
		return NULL;
}


DestroyFunc map_set_destroy_key(Map map, DestroyFunc destroy_key) {
	DestroyFunc old = map->destroy_key;
	map->destroy_key = destroy_key;
	return old;
}

DestroyFunc map_set_destroy_value(Map map, DestroyFunc destroy_value) {
	DestroyFunc old = map->destroy_value;
	map->destroy_value = destroy_value;
	return old;
}

// Απελευθέρωση μνήμης που δεσμεύει το map
void map_destroy(Map map) {


	for (int i = 0; i < map->capacity; i++) {
		if (map->array[i].state == OCCUPIED) {
			if (map->destroy_key != NULL)
				map->destroy_key(map->array[i].key);
			if (map->destroy_value != NULL)
				map->destroy_value(map->array[i].value);
		}
	}
	for (int i = 0; i < map->capacity; i++) {
		if (vector_get_at(map->chaining, i) != NULL) {
			Vector chain_vector = vector_get_at(map->chaining, i);
			for (VectorNode node = vector_first(chain_vector);
			node != VECTOR_EOF;
			node = vector_next(chain_vector, node)) {
				MapNode temp = vector_node_value(chain_vector, node);
				if(temp->state == OCCUPIED){
					if (map->destroy_key != NULL)
						map->destroy_key(temp->key);
					if (map->destroy_value != NULL)
						map->destroy_value(temp->value);
				}
				free(temp);
			}
			vector_destroy(chain_vector);
		}
	}

	

	vector_destroy(map->chaining);
	free(map->array);
	free(map);
}

/////////////////////// Διάσχιση του map μέσω κόμβων ///////////////////////////

MapNode map_first(Map map) {
	//Ξεκινάμε την επανάληψή μας απο το 1ο στοιχείο, μέχρι να βρούμε κάτι όντως τοποθετημένο
	for (int i = 0; i < map->capacity; i++)
		if (map->array[i].state == OCCUPIED)
			return &map->array[i];

	return MAP_EOF;
}

MapNode map_next(Map map, MapNode node) {
	bool start_chain = false;    //Flag για όταν το iteration πρέπει να ξεκινήσει από το 0

    if (node->flag == false){
        // Το node είναι pointer στο i-οστό στοιχείο του array, οπότε node - array == i  (pointer arithmetic!)
        for (int i = node - map->array + 1; i < map->capacity; i++){
            if (map->array[i].state == OCCUPIED){
                return &map->array[i];
            }
        }
        start_chain = true;    //Ξεκίνα το chain iteration από το πρώτο chain, το επόμενο δεν είναι μέσα στο βασικό array
    }

    //Chain iteration
    bool searching_next = start_chain ? true : false;    //Θα πρέπει πρώτα να βρούμε τον κόμβο που δίνεται για να βρούμε τον επόμενο
    //Αν ερχόμαστε από τον βασικό Array, ο πρώτος κατειλημμένος κόμβος με find είναι αυτό που χρειαζόμαστε
    for (int chain_cnt = start_chain ? 0 : map->hash_function(node->key) % map->capacity;
        chain_cnt < map->capacity;
        chain_cnt++){

        if (vector_get_at(map->chaining, chain_cnt) != NULL){    // Υπάρχει vector?
            for (VectorNode snode = vector_first(vector_get_at(map->chaining, chain_cnt));    
                snode != VECTOR_EOF;
                snode = vector_next(vector_get_at(map->chaining, chain_cnt), snode)) {
                    MapNode node2 = vector_node_value(vector_get_at(map->chaining, chain_cnt), snode);
                    if (searching_next && node2->state == OCCUPIED)
                        return node2;
                    else if(map->compare(node2->key, node->key) == 0 && node2->state == OCCUPIED){
                        searching_next = true;
                    }
                
                }
        }    
    }
    return MAP_EOF;
}

Pointer map_node_key(Map map, MapNode node) {
	return node->key;
}

Pointer map_node_value(Map map, MapNode node) {
	return node->value;
}

MapNode map_find_node(Map map, Pointer key) {
	// Διασχίζουμε τον πίνακα, ξεκινώντας από τη θέση που κάνει hash το key, και για όσο δε βρίσκουμε EMPTY
	int count = 0;
	for (uint pos = map->hash_function(key) % map->capacity;		// ξεκινώντας από τη θέση που κάνει hash το key
		count <= NEIGHBOURS;
		pos = (pos + 1) % map->capacity, count++) {						// linear probing, γυρνώντας στην αρχή όταν φτάσουμε στη τέλος του πίνακα
		// Μόνο σε OCCUPIED θέσεις (όχι DELETED), ελέγχουμε αν το key είναι εδώ
		if (map->array[pos].state == OCCUPIED && map->compare(map->array[pos].key, key) == 0)
			return &map->array[pos];
	}

	if (vector_get_at(map->chaining, map->hash_function(key) % map->capacity) != NULL){
		for (VectorNode node = vector_first(vector_get_at(map->chaining, map->hash_function(key) % map->capacity));
		node != VECTOR_EOF;
		node = vector_next(vector_get_at(map->chaining, map->hash_function(key) % map->capacity), node)){
			MapNode temp = vector_node_value(vector_get_at(map->chaining, map->hash_function(key) % map->capacity), node);
			if (map->compare(temp->key, key) == 0 && temp->state == OCCUPIED)
				return temp;
		}
	}

	return MAP_EOF;
}

// Αρχικοποίηση της συνάρτησης κατακερματισμού του συγκεκριμένου map.
void map_set_hash_function(Map map, HashFunc func) {
	map->hash_function = func;
}

uint hash_string(Pointer value) {
	// djb2 hash function, απλή, γρήγορη, και σε γενικές γραμμές αποδοτική
    uint hash = 5381;
    for (char* s = value; *s != '\0'; s++)
		hash = (hash << 5) + hash + *s;			// hash = (hash * 33) + *s. Το foo << 5 είναι γρηγορότερη εκδοχή του foo * 32.
    return hash;
}

uint hash_int(Pointer value) {
	return *(int*)value;
}

uint hash_pointer(Pointer value) {
	return (size_t)value;				// cast σε sizt_t, που έχει το ίδιο μήκος με έναν pointer
}