/* Decoding program for LZ78-style compression regime.
 * Input is plain text
 * from stdin.
 * Output is multiple lines by stdout with 2 component each line
 *  -- a single character, the "extension"
 *  -- a single integer, the "copystring"
 * 
 * This program is using dictionary binary tree.
 * However, this dictionay binary tree is not a normal dict binary tree
 * 
 * Recall how string is inserted into the tree, there are some characteristics:
 *  -- a char longer than a string already exists in the dictionary
 *  -- going to the right if and only if the prefix of the searching string is
 *     same as the prefix of the string of the node
 * and the tree have some characteristics as well:
 *  -- the string on the right of a node, will always longer or equal to the
 *     string of that node
 *  -- extend of the above charisteristic
 *     -- if they have same length, definitely, they have same prefix.
 *         which implied that with different prefix, it is definitely smaller
 * 
 * In this program, tree data contain 3 parts, the prefix pointer, a char,
 * and an index
 * which makes comparying very easy.
 *  -- different prefix -> smaller
 *     -- just one except case, if the searching prefix is equal to the whole
 *         string of the node (there is a way to prevent this fault)
 *  -- same prefix -> compare the char
 *  
 * and there is a way to save searching time (which can prevent the fault
 * above), and of course save inserting time as well
 *  -- everytime found -> remember that node, and continue searching from the 
 *     right of it when next char read in
 *     (searching from right can already prevent the problem)
 *
 * The binary search tree implementation is based on
 * Professor Alistair Moffat's
 * http://people.eng.unimelb.edu.au/ammoffat/ppsaa/c/treeops.c
 * http://people.eng.unimelb.edu.au/ammoffat/ppsaa/c/treeops.h
 * 
 * AND FINALLY ALGORITHMS ARE FUN. Yes, Very FUN!
 * 
 * Author:      Shing Sheung Daniel Ip
 * StudentID:   723508
 * Purpose:     Assignment 2 solution for 2015 Sem 2 COMP10002 Foundation of 
 *              Algorithm in University of Melbourne
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* Return variable for main process (getchar_process) */
#define ENDPROCESS 0
#define CONTPROCESS 1
#define CUTOFF 0
#define LESSTHAN -1
#define GREATER 1
#define RESET 0

/* type for tree and data */
typedef struct node node_t;

struct node {
	void *data;              /* ptr to stored structure */
	node_t *left;            /* left subtree of node */
	node_t *rght;            /* right subtree of node */
};

typedef struct {
	node_t *root;            /* root node of the tree */
	int (*cmp)(void*,void*); /* function pointer */
} tree_t;

typedef struct {
	int temp;     /* storing the index of prefix node */
	int pre;      /* storing the index of the prefix node of prefix node */
	int count;    /* counter for the number of factors */
} index_t;

/* data_t is storing prefix_node and ch, which can always fully
 * define the string for LZ78
 */
typedef struct {
	node_t *prefix_node;
	int index;
	char ch;
} data_t;

/* function prototypes */
tree_t *make_empty_tree(int func(void*,void*));
tree_t *insert_in_order(tree_t *tree, void *value);
void free_tree(tree_t *tree);

static void *recursive_search_tree(node_t*, void*, int(void*,void*));
static node_t *search_branch(tree_t *tree, void *key, void *branch);
static void recursive_free_tree(node_t*);

static int dictcmp(void*, void*);
static int getchar_process(tree_t*, index_t*, node_t**, char**, int*);
static void process_char(tree_t*, index_t*, node_t**, char**);
static index_t *init_index();
static tree_t *insert_in_order_branch(tree_t *tree, void *value,
	void *branch);
static void free_temp_char(char**);

/**************************************************************************/

/* main program create all necessary variable and calling main process
 */
int main(int argc, char *argv[]){
	// initialise the tree
	tree_t *dict=make_empty_tree(dictcmp);
	index_t *index=init_index();
	
	// make a temp node so that I can reuse in the while loop
	// therefore not everytime searching from the root
	node_t **temp_node;
	temp_node=(node_t **)malloc(sizeof(*temp_node));
	assert(temp_node!=NULL);
	*temp_node=NULL;

	// the current char
	char **temp_ch;
	temp_ch=(char**)malloc(sizeof(*temp_ch));
	assert(temp_ch!=NULL);
	*temp_ch=NULL;

	// storing how many char has been processed
	int ch_count=0;

	while(getchar_process(dict, index, temp_node, temp_ch, &ch_count)
		!=ENDPROCESS);

	fflush(stdout);
	fprintf(stderr, "encode: %6d bytes input\n", ch_count);
	fprintf(stderr, "encode: %6d factors generated\n", index->count);

	// free everything and make them NULL, so not producing bugs
	free_tree(dict); free(temp_node); free(temp_ch); free(index);
	dict=NULL; temp_node=NULL; temp_ch=NULL; index=NULL;

	return 0;
}

/**************************************************************************/

/* function to make the initial empty tree
 */
tree_t *make_empty_tree(int func(void*,void*)) {
	tree_t *tree;
	tree = (tree_t*)malloc(sizeof(*tree));
	assert(tree!=NULL);
	/* initialize tree to empty */
	tree->root = NULL;
	/* and save the supplied function pointer */
	tree->cmp = func;
	return tree;
}

/**************************************************************************/

/* function to initiallize index for counting
 */
static index_t *init_index(){
	index_t *index;
	index=(index_t*)malloc(sizeof(*index));
	assert(index!=NULL);
	index->temp=RESET;
	index->pre=RESET;
	index->count=RESET;
	return index;
}

/**************************************************************************/

/* The following 2 functions are for searching, recursive search for the
 * dictionary tree.
 */
static void *recursive_search_tree(node_t *root, void *key,
	int cmp(void*,void*)) {
	int outcome;
	if (!root) {
		return NULL;
	}
	if ((outcome=cmp(key, root->data)) < CUTOFF) {
		return recursive_search_tree(root->left, key, cmp);
	} else if (outcome > CUTOFF) {
		return recursive_search_tree(root->rght, key, cmp);
	} else {
		/* hey, must have found it! */
		return root;
	}
}
/* search_branch can perform searching from root when then branch is NULL
 * and also searching from the right of the branch if the branch is not NULL
 */
static node_t *search_branch(tree_t *tree, void *key, void *branch) {
	assert(tree!=NULL);
	if(branch==NULL)
		return (node_t*)recursive_search_tree(tree->root, key, 
			tree->cmp);
	else
		return (node_t*)recursive_search_tree(((node_t*)branch)->rght, 
			key, tree->cmp);
}

/**************************************************************************/

/* following 2 function for inserting the node into the tree */

/* function to insert node into the appropriate position recusively
 */
static node_t *recursive_insert(node_t *root, node_t *node,
	int cmp(void*,void*)) {
	if (root==NULL) {
		return node;
	} else if (cmp(node->data, root->data) < CUTOFF) {
		root->left = recursive_insert(root->left, node, cmp);
	} else {
		root->rght = recursive_insert(root->rght, node, cmp);
	}
	return root;
}

/* insert function handle both inserting from root and branch
 */
tree_t *insert_in_order_branch(tree_t *tree, void *value, void *branch) {
	node_t *node;
	/* make the new node */
	node = (node_t*)malloc(sizeof(*node));
	assert(tree!=NULL && node!=NULL);
	node->data = value;
	node->left = node->rght = NULL;
	/* and insert it into the tree
	 * when branch is NULL, meaning insert from root
	 * when branch is not NULL, insert from right of the branch
	 */
	if(branch==NULL)
		tree->root = recursive_insert(tree->root, node, tree->cmp);
	else
		((node_t*)branch)->rght = recursive_insert(
			((node_t*)branch)->rght, node, tree->cmp);
	return tree;
}

/**************************************************************************/

/* the following 2 functions for freeing the tree*/

/* recursively free the tree
 */
static void recursive_free_tree(node_t *root) {
	if (root) {
		recursive_free_tree(root->left);
		recursive_free_tree(root->rght);
		free(root);
	}
}

/* Release all memory space associated with the tree structure
 */
void free_tree(tree_t *tree) {
	assert(tree!=NULL);
	recursive_free_tree(tree->root);
	free(tree);
}

/**************************************************************************/

/* comparing function for dictionary binary tree
 * comparing logic obtained with paperwork
 * briefly if the prefix indexes are the same, then compare the char
 * if the prefix indexes are not the same, then going to the left
 */
static int dictcmp(void *key, void *node){
	if(((data_t*)key)->prefix_node!=((data_t*)node)->prefix_node)
		return LESSTHAN;
	else
		return ((((data_t*)key)->ch)-(((data_t*)node)->ch));
}

/**************************************************************************/

/* Main function to get char and pass it to the char processing function
 * and handle the last char while EOF
 */
static int getchar_process(tree_t *dict, index_t *index,
	node_t **temp_node, char **temp_ch, int *ch_count){
	assert(dict!=NULL && dict!=NULL && temp_node!=NULL);
	char c;
	if((c=getchar())!=EOF){
		/* storing the char in pointer instead of just passing the char
		 * mainly to manage the last char in the file (using pointer to 
		 * char to store the char before current char)
		 */
		if(*temp_ch==NULL){
			*temp_ch=(char*)malloc(sizeof(**temp_ch));
			assert(*temp_ch!=NULL);
		}
		**temp_ch=c;

		/* process the char */
		process_char(dict,index, temp_node, temp_ch);

		/* return continue */
		(*ch_count)++;
		return CONTPROCESS;
	}else{
		/* if temp_ch (storing second last char) is not empty, 
		 * then print out the char with previous index
		 */
		if(*temp_ch){
			printf("%c%d\n", **temp_ch, index->pre);
			free_temp_char(temp_ch);
			(index->count)++;
		}

		/* return END */
		return ENDPROCESS;
	}
}

/**************************************************************************/

/* function to process the char
 * NOT NECESSARY TO PROCESS THE STRING WITH THE DATA STRUCT IN THIS PROGRAM
 */
static void process_char(tree_t *dict, index_t *index,
	node_t **temp_node, char **temp_ch){
	assert(dict!=NULL && temp_node!=NULL);

	/* malloc for temp_data, which might be inserting into the tree if 
	 * cannot be found in the tree
	 */
	data_t *temp_data;
	temp_data = (data_t*)malloc(sizeof(*temp_data));
	assert(temp_data!=NULL);
	temp_data->ch=**temp_ch;
	temp_data->index=index->count+1;
	temp_data->prefix_node=*temp_node;

	/* local variable to store temp node, just before searching, 
	 * just prepare for inserting.
	 */
	node_t *local_temp_node;
	local_temp_node = *temp_node;

	/* search the tree */
	*temp_node=search_branch(dict, temp_data, *temp_node);
	if(*temp_node==NULL){
		/* if not found, must add it into the tree, and as it is new 
		 * string, print out this factor
		 */
		printf("%c%d\n", **temp_ch, index->temp);

		/* insert the data into the tree */
		dict=insert_in_order_branch(dict, temp_data, local_temp_node);
		
		/* count the number of factors and reset the variables */
		index->count++;
		index->temp=RESET;
		index->pre=RESET;
		*temp_node=NULL;
		free_temp_char(temp_ch);
	}else{
		/* free the temp_data to prevent mem-leak, and record the  
		 * found node
		 */
		free(temp_data);
		temp_data=NULL;
		index->pre=index->temp;
		index->temp=((data_t*)((node_t*)*temp_node)->data)->index;
	}
}

/**************************************************************************/

/* function to free temp char
 */
static void free_temp_char(char **temp_ch){
	assert(temp_ch!=NULL);
	free(*temp_ch);
	*temp_ch=NULL;
}
