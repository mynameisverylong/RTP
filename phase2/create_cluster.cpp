/** This project is used to generate clusters
 *  Author: Jiyu
 **/
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>
#include <stdlib.h>
#include "neverlost_util.h"
using namespace std;

unordered_map<string,int> title_to_did;
unordered_map<int,vector<int>> did_to_vids;
int doc_num;

int total_found_fragment;
int did, next_did; // document id
int vid, next_vid; // version id
int fid, next_fid; // fragment id
unordered_map<string, int> title_did_hash; // hash from title to document id
unordered_map<int, int> vid_did_hash; // hash from vid to did
unordered_map<int, vector<int>> did_vid_hash; // hash from did to vid; each document will have a vector of vid
unordered_map<int, int> fid_size_hash; // hash from fid to its size
unordered_map<int, string> fid_content_hash; // hash from fid to its content
unordered_map<string, int> content_fid_hash; // hash from a fragment's content to its id
unordered_map<int, vector<int>> vid_fid_hash; // hash from vid to fid; each page will have a vector of fragment
unordered_map<int ,string> did_title_hash; // hash from did to title 
ofstream latest_version;

class ReuseTableInfo
{
public:
    int vid;
    int offset;
};


unordered_map<int, vector<ReuseTableInfo>> frag_reuse_table;


void init()
{
    doc_num=0;
    title_to_did.clear();
    did_to_vids.clear();
    system("rm -r cluster");
    system("mkdir cluster");
}

void deal_with_ver(string& content, int vid)
{
    size_t found = content.find_first_of("\t");
    size_t len = found + 1;
    string title;
    title = content.substr(0, len);
    int current_doc;
    auto iter = title_to_did.find(title);
    if (iter == title_to_did.end()){
        title_to_did[title] = doc_num;
        current_doc = doc_num;
        doc_num++;

        did_to_vids[current_doc].clear();

        ostringstream command;
        command << "mkdir ./cluster/" << current_doc;
        string commd = command.str();
        system(commd.c_str());

    }
    else {
        current_doc = iter->second;
    }

    did_to_vids[current_doc].push_back(vid);


    ostringstream outputfile;
    outputfile << "./cluster/" << current_doc << "/all_ver.txt";
    string filename = outputfile.str();
    ofstream fout;

    fout.open(filename.c_str(), ofstream::app);
    fout << content << endl;
    fout.close();
}

void cluster_init()
{
    //total_found_fragment=0;
    next_did = next_vid = next_fid = 0;
    title_did_hash.clear();
    vid_did_hash.clear();
    did_vid_hash.clear();
    fid_size_hash.clear();
    fid_content_hash.clear();
    content_fid_hash.clear();
    vid_fid_hash.clear();
    frag_reuse_table.clear();
    did_title_hash.clear();
}

/** location of the docs in version $i: ../fragment/result$(i+1)/XX.txt
 **/

// find the document id of a page given its title
int get_did_from_title(string &title)
{
    int id;
    unordered_map<string, int>::const_iterator iter = title_did_hash.find(title); 
    if (iter != title_did_hash.end()) // find docid existing in current hash
        id = iter->second; 
    else // not found
    {
        id = next_did; 
        next_did++;
        title_did_hash[title] = id;
    }
    return id;
}

// maintain did_vid_hash
void maintain_did_vid_hash()
{
    unordered_map<int, vector<int>>::iterator iter = did_vid_hash.find(did);
    if (iter != did_vid_hash.end()) // found
    {
        vector<int> *p;
        p = &(iter->second);
        p->push_back(vid);
    }
    else // not found
    {
        vector<int> new_vector;
        new_vector.clear();
        new_vector.push_back(vid);
        did_vid_hash[did] = new_vector;
    }
}

// calculate the number of words in a string
int get_words_number(string &line)
{
    int number=1;
    for (int i=0; i<line.length(); i++)
    {
        if (line[i] == ' ')
            number++;
    }
    return number;
}

void output_fragment(int fid, string &line, string& folder_base)
{
    ofstream fout;
    string file_name = folder_base+"all_fragments.txt";
    fout.open(file_name.c_str(), ofstream::app);
    fout << line << endl;
    fout.close();
}

void deal_with_title(string title, int pageno)
{
    // If the title is empty, we don't deal with this page. Regard it as an error one.    
    if (title.length() < 2)
    {
        cout << "Empty title at page " << pageno << ". Disregard it!"<< endl;
        return;
    }
    // If we get the title correctly, deal with this page.
    did = get_did_from_title(title);
    // maintain vid_did_hash
    vid_did_hash[vid] = did;
    // maintain did_title_hash
    did_title_hash[did] = title;
    // maintain did_vid_hash: add vid to the list of did
    maintain_did_vid_hash();
}

// do the work for all the fragments
void do_fragments(vector<string>& frag, int pageno, string& folder_base)
{
    string line;
    if (frag.size()==0)
        return;
    int current_offset=0; // cumulated words offset within page
    int frag_words_number=0; // the words number of current fragment

    for (int lnum=0;lnum<frag.size();lnum++)
    {
        // each time, get a line, it is a fragment
        // Step 1, get the fid: check if this fragment occurs before. 
        // If yes, get the old fid; otherwise, give it a new fid.
        line = frag[lnum];
        if (lnum==0)
            deal_with_title(line,pageno);
        unordered_map<string, int>::const_iterator iter = content_fid_hash.find(line);
        if (iter != content_fid_hash.end()) // found; If we have saved this fragment before, we don't need to maintain this fragment's info.
        {
            total_found_fragment++;
            fid = iter->second;
            frag_words_number = fid_size_hash[fid];
        }
        else // not found
        {
            fid = next_fid++; // give it a new fid
            // maintain this fragment's info
            content_fid_hash[line] = fid;
            fid_content_hash[fid] = line;
            frag_words_number = get_words_number(line); // If it's the first time we meet this fragment, we must calculate its size.
            fid_size_hash[fid] = frag_words_number;
            // output this fragment
            output_fragment(fid, line, folder_base);
        }

        // Step 2, maintain vid_fid_hash
        unordered_map<int, vector<int>>::iterator iter2 = vid_fid_hash.find(vid);
        if (iter2 != vid_fid_hash.end()) // found
        {
            vector<int> *p2 = &(iter2->second);
            p2->push_back(fid);
        }
        else // not found
        {
            vector<int> new_vector;
            new_vector.clear();
            new_vector.push_back(fid);
            vid_fid_hash[vid] = new_vector;
        }

        // Step 3, maintain frag_reuse_table
        ReuseTableInfo new_element;
        new_element.vid = vid;
        new_element.offset = current_offset;
        
        unordered_map<int, vector<ReuseTableInfo>>::iterator iter3 = frag_reuse_table.find(fid);
        if (iter3 != frag_reuse_table.end()) // found
        {
            vector<ReuseTableInfo> *p = &(iter3->second);
            p->push_back(new_element);
        }
        else // not found
        {
            vector<ReuseTableInfo> new_vector;
            new_vector.clear();
            new_vector.push_back(new_element);
            frag_reuse_table[fid] = new_vector;
        }

        // At last, add frag_words_number to current_offset
        current_offset += frag_words_number;
    }
}

// do the work for a page which can be acquired using the input file stream "fin"
void do_page(vector<string>& frag, int pageno, string& folder_base)
{
    vid = next_vid++;
    
    // deal with the fragments now
    do_fragments(frag, pageno, folder_base);
}


char hash_string(string s)
{
    int result=0;
    for (int i=0; i<s.size(); i++)
    {
        result += s[i];
        result %= 127;
    }
    return (char)(result+1);
}

void hash_content(char *result_hash, vector<string>& v)
{
    int i;
    for (i=0; i<v.size(); i++)
        result_hash[i] = hash_string(v[i]);
    result_hash[i] = '\0';
}



void deal_with_doc(string& content, int doc_id, string& folder_base)
{
    /* Deal with each document:
     * (1) Firstly we need to hash each string to a char.
     * (2) Then we partition on this char array.
     * (3) Lastly we restore the fragment on doc words.
     */ 
    // Step(1):
    // find title
    size_t found = content.find_first_of("\t");
    size_t len = found + 1;
    string title;
    title = content.substr(0, len);
    content = content.substr(len); // don't print title again in the second line
    // read strings of a doc
    istringstream iss(content);
    vector<string> v;
    v.clear();
    string word;
    while (iss >> word)
    {
        v.push_back(word);
    }
    int word_number = v.size();
    char *result_hash = new char[word_number+1];
    hash_content(result_hash, v);
    
    // Step(2):
    string char_doc(result_hash);
    vector<string> chunks;
    NeverLostUtil::chunkContent(char_doc, true, 30, chunks); 
    
    // Step(3):
    ofstream fout;
    int cur_start=0;
    
    // form result
    vector<string> doc_result(0);
    // print title as the first line
    doc_result.push_back(title);
    for (int i=0; i<chunks.size(); i++) 
    {
        string fragment="";
        for (int j=0; j<chunks[i].size(); j++)
        {
            fragment += (j==0?"":" ");
            fragment += v[cur_start++];
        }
        doc_result.push_back(fragment);
    }
    do_page(doc_result, doc_id, folder_base);
    delete []result_hash;
}


void output_index(string& folder_base)
{
    ofstream findex;
    string file_name = folder_base+"index.txt";
    findex.open(file_name.c_str());
    // output index in txt format
    // output reuse_table
    unordered_map<int, vector<ReuseTableInfo>>::iterator iter;
    for (iter = frag_reuse_table.begin(); iter != frag_reuse_table.end(); iter++)
    {
        findex << iter->first << " ";
        vector<ReuseTableInfo> v;
        v = iter->second;
        findex << v.size() << " ";
        for (int i=0; i<v.size(); i++){
            findex << v[i].vid << " " << v[i].offset << " ";
        }
        findex << endl;
    }
    findex << -99999 << endl; // index separator
    // output vid title hash
    unordered_map<int ,int>::iterator iter2; // iteration of vid_did_hash
    for (iter2 = vid_did_hash.begin(); iter2 != vid_did_hash.end(); iter2++)
    {
        string title = did_title_hash[iter2->second];
        // calculate how many words
        int words_number=1;
        for (int i=0; i<title.length(); i++)
            if (title[i] == ' ')
                words_number++;
        findex << iter2->first << " " << words_number<< endl;
    }
    findex << -99999 << endl; // index separator
    findex.close();
}

// output the relationship between version and document
void output_dvrelation(string& folder_base)
{
    ofstream freldv, frelvd;
    string freldv_file = folder_base+"dvrelation.txt";
    string frelvd_file = folder_base+"vdrelation.txt";
    freldv.open(freldv_file.c_str());
    frelvd.open(frelvd_file.c_str());
    // output vid_did_hash
    unordered_map<int, int>::iterator iter;
    for (iter = vid_did_hash.begin(); iter != vid_did_hash.end(); iter++)
    {
        frelvd << iter->first << " " << iter->second << endl;
    }
    // output did_vid_hash
    unordered_map<int, vector<int>>::iterator iter2;
    for (iter2 = did_vid_hash.begin(); iter2 != did_vid_hash.end(); iter2++)
    {
        freldv << iter2->first << " ";
        vector<int> vid_vec = iter2->second;
        freldv << vid_vec.size() << " ";
        for (int i=0; i<vid_vec.size(); i++)
            freldv << vid_vec[i] << " ";
        freldv << endl;
    }
    frelvd.close();
    freldv.close();
}


void gen_index_for_cluster(string& folder_base){

    string filename = folder_base + "all_ver.txt";
    ifstream fin;
    fin.open(filename.c_str());
    string line; // each line of the input file is a document
    int doc_num=0;
    cluster_init();
    string latest_line;
    while(getline(fin, line))
    {
        // deal with each document
        latest_line = line;
        deal_with_doc(line, doc_num, folder_base);
        doc_num++;
    }
    latest_version << latest_line << endl;
    fin.close();
    output_index(folder_base);
    output_dvrelation(folder_base);
}

int output_convert_table(){
    ofstream convert_table;
    string convert_path = "./convert_table.txt";
    convert_table.open(convert_path.c_str(), ios::ate);
    for (int i=0; i < doc_num; i++){
        convert_table << i << " " << did_to_vids[i].size();
        for (int j=0; j < did_to_vids[i].size(); j++){
            convert_table << " " << did_to_vids[i][j];
        }
        convert_table << endl;
    }
    convert_table.close();
    return 0;
}

int main(int argc, char** argv) {
    if (argc != 2)
    {
        cout << "need filename" << endl;
        exit(1);
    }
    string filename = string(argv[1]);
    ifstream fin;
    fin.open(filename.c_str());
    string latest_name = "../phase1_super/latest.txt";
    latest_version.open(latest_name.c_str(), ios::ate);
    string line; // each line of the input file is a document
    int vid=0;
    init();
    while(getline(fin, line))
    {
        // deal with each document
        deal_with_ver(line, vid);
        vid++;
        if (vid%10000==0)
            printf("done vid = %d\n",vid);
    }
    fin.close();

    for (int i=0;i<doc_num;i++){
        cluster_init();
        ostringstream folder_base;
        folder_base << "./cluster/" << i << "/";
        string base = folder_base.str();
        gen_index_for_cluster(base);
        if (i%1000==0)
            printf("done doc = %d\n",i);
    }
    latest_version.close();
    output_convert_table();
    return 0;
}


