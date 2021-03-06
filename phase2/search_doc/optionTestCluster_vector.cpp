#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <iterator>
#include <math.h>
#include <ctime>
#include <sys/time.h>

// This version replaces all set with vector

using namespace std;

// fid occurence
class Occurence
{
public:
    int fid;
    vector<int> v_pos;
};

// vid occurence
class V_Occurence
{
public:
    int vid;
    vector<int> pos;
};

class ForwardTableInfo
{
public:
    int fid;
    int offset;
};

class ReuseTableInfo
{
public:
    int vid;
    int offset;
};

class ScoreResult
{
public:
    int vid;
    double score;
};

const int MAX_VID = 5500000;//max number of VID
const int y = 13; // average number of fragments in one page: 13 wiki, 22 web, 17 git

unordered_map<int, vector<ReuseTableInfo>> frag_reuse_table;
unordered_map<int ,int> vid_titlelen_hash; // vid to title length
vector<ForwardTableInfo> forward_table[MAX_VID];
vector<vector<Occurence>> search_frag; // <<fID, <pos>>>
vector<int> vid_list; // vid list
unordered_map<int, vector<vector<int>>*> vid_posting; // vid_posting, for each vid, there will be a posting
vector<vector<V_Occurence>> doc_posting; // each term, a list of vid_occurence, result of step 1 is saved in this.
unordered_map<int, vector<set<int>>> intersection_hash; // this keeps the intersection results
vector<ScoreResult> score_result; // this vector keeps the final scoring results

int query_len;

void read_index()
{
    ifstream fin;
    fin.open("index.txt");
    // read frag_reuse_table
    frag_reuse_table.clear();
    int fid, size, vid, offset;
    while(fin >> fid) // deal with one line
    {
        if (fid == -99999) // index separator
            break;
        fin >> size;
        vector<ReuseTableInfo> v;
        v.clear();
        for (int i=0; i<size; i++)
        {
            fin >> vid >> offset;
            ReuseTableInfo r;
            r.vid = vid;
            r.offset = offset;
            v.push_back(r);
        }
        frag_reuse_table[fid] = v; // create one entry
    }
    int title_len;
    while(fin >> vid)
    {
        if (vid == -99999) // index separator
            break;
        fin >> title_len;
        vid_titlelen_hash[vid] = title_len;
    }
    fin.close();
}

// print search_frag in txt format
void validate()
{
    ofstream fout;
    fout.open("validate.txt");
    for (int i=0; i<search_frag.size(); i++)
    {
        vector<Occurence> v = search_frag[i];
        for (int j=0; j<v.size(); j++)
        {
            Occurence o = v[j];
            fout << "term " << i << " Occ " << j << " fid " << o.fid << " pos ";
            for (int k=0; k<o.v_pos.size(); k++)
                fout << o.v_pos[k] << " ";
            fout << endl;
        }
    }
    fout.close();
}

void print_vid_postingC()
{
    ofstream fout;
    fout.open("posting.txt");
    unordered_map<int, vector<vector<int>>*>::iterator iter;
    for (iter = vid_posting.begin(); iter != vid_posting.end(); iter++){
        fout << iter->first << " ";
        vector<vector<int>> *v = iter->second;
        for (int i=0; i<(*v).size(); i++){
            fout << "term " << i << ": ";
            for (int j=0; j<(*v)[i].size(); j++)
                fout << (*v)[i][j] << " ";
        }
        fout << endl;
    }
    fout.close();
}

void print_vid_postingA()
{
    ofstream fout;
    fout.open("posting.txt");
    for (int i=0; i<query_len; i++)
    {
        fout << "term " << i <<": ";
        //vector<vector<V_Occurence>> doc_posting; // each term, a list of vid_occurence, result of step 1 is saved in this.
        vector<V_Occurence> *current_posting = &(doc_posting[i]);
        for (int j=0; j<current_posting->size(); j++)
        {
            int v = (*current_posting)[j].vid;
            vector<int> *p = &((*current_posting)[j].pos);
            fout << "vid " << v <<" has pos ";
            vector<int>::iterator iter;
            for (iter = p->begin(); iter != p->end(); iter++)
                fout << *iter << " ";
        }
        fout << endl;
    }
    fout.close();
}

bool mycmp(const Occurence& o1, const Occurence& o2)
{
    return (o1.fid < o2.fid);
}

bool mycmpA(const V_Occurence &obj1, const V_Occurence &obj2)
{
        return obj1.vid < obj2.vid;
}

// read the postings of terms and sort them by fid
void read_search_frag()
{
    ifstream fin;
    fin.open("search_frag.txt");
    search_frag.clear();
    int fid, pos, term;
    int previous_term=-1;
    while(fin >> term) // each time deal with an occurence
    {
        if (term != previous_term) // meet a new term
        {
            if (term != previous_term+1)
            {
                cout << "missing frags for term " << previous_term+1 << endl;
                exit(1);
            }
            vector<Occurence> cur_v;
            cur_v.clear();
            search_frag.push_back(cur_v);
        }
        vector<Occurence> *v;
        v = &(search_frag[term]);
        Occurence occ;
        fin >> occ.fid;
        occ.v_pos.clear();
        // push_back all the positions
        while((fin >> pos) && (pos != -1))
        {
            occ.v_pos.push_back(pos);
        }
        // push back an occurence
        v->push_back(occ);
        previous_term = term;
    }
    if (previous_term != query_len-1)
    {
        cout << "missing frags for term " << query_len-1 << endl;
        exit(1);
    }

    fin.close();

    // sort search_frag to make each posing sorted by fid
    for (int i=0; i<query_len; i++){
        sort(search_frag[i].begin(), search_frag[i].end(), mycmp);
        cout << "Term " << i << " contains " << search_frag[i].size() << " postings" << endl;
    }
    //validate();
}

// read vids
void read_vid()
{
    ifstream fin;
    fin.open("vidlist.txt");
    vid_list.clear();
    int v;
    while(fin >> v)
        vid_list.push_back(v);
    cout << "Read "<< vid_list.size() << " vids from previous intersection results."<< endl;
    fin.close();
}

// read forward reuse table
void read_forward()
{
    ifstream fin;
    fin.open("forward.txt");
    int vid, size, fid, offset;
    int count=0;
    while(fin >> vid) // deal with one line
    {
        count++;
        fin >> size;
        vector<ForwardTableInfo> v;
        v.clear();
        for (int i=0; i<size; i++)
        {
            fin >> fid >> offset;
            ForwardTableInfo r;
            r.fid = fid;
            r.offset = offset;
            v.push_back(r);
        }
        forward_table[vid] = v; // create one entry for vid
    }
    fin.close();
    cout << "Read " << count << " forward reuse table entries in forward_table." << endl;
}

// binary search within [start, end] using recursion
int find_index_old(vector<Occurence> *term_posting, int fid, int start, int end)
{
    if (start > end)
        return -1; // error! fid not in term_posting
    int mid = (start+end)/2;
    if ((*term_posting)[mid].fid == fid)
        return mid;
    else if (fid < (*term_posting)[mid].fid)
        return find_index_old(term_posting, fid, start, mid-1);
    else
        return find_index_old(term_posting, fid, mid+1, end);
}

int find_index(vector<Occurence> *term_posting, int fid, int start, int end)
{
    while(start<=end){
        int mid = (start+end)/2;
        if ((*term_posting)[mid].fid == fid)
            return mid;
        else if (fid < (*term_posting)[mid].fid)
            end = mid - 1;
        else
            start = mid + 1;
    }
    return -1;
}

bool MakeChoice(int k)
{
    vector<Occurence> v = search_frag[k];
    int f = v.size();
    int doc_sum = 0;
    int pos_sum = 0;
    for(int i=0;i<f;i++)
    {
        Occurence o = v[i];
        doc_sum += frag_reuse_table[o.fid].size();
        pos_sum = pos_sum + o.v_pos.size();
    }
    int u = doc_sum/f;
    double p = pos_sum*1.0/f;
    int m = vid_list.size();
    int frag_sum = 0;
    for(int i=0;i<m;i++)
    {
        frag_sum += forward_table[vid_list[i]].size();
        //frag_sum += forward_table[vid_list[i]].size() + f;
    }
    int pi = frag_sum/m;
    bool res = doc_sum*50<frag_sum*(1+(int)log2(f));
    //bool res = doc_sum*50<frag_sum;
    return res;
}



// calculate the positional information for each vid
// each vid: vector<set<int>>, each term corresponds to a set of positions
void get_positional_info()
{
    // begin timing
    struct timeval dwstart, dwend, dwstart_sort, dwend_sort, dwstartA, dwendA, dwstartC, dwendC;
    double dwtime = 0, dwtime_sort=0, dwtimeA=0, dwtimeC=0;
    int vid, fid, frag_offset;
    // init Option C's data structures
    vector<vector<int>> *e = new vector<vector<int>>[vid_list.size()];
    for (int i=0; i<vid_list.size(); i++)
        e[i].resize(query_len);
    bool flag_do_optionA = 1;
    // init Option A's data structures
    int term_number = query_len;
    //vector<vector<V_Occurence>> doc_posting(term_number); // each term, a list of vid_occurence, result of step 1 is saved in this.
    doc_posting.resize(term_number);
    doc_posting.clear();

    // begin optionTest algorithm 
    double ratio;
    int y;
    if (query_len<5)
        ratio = (5.0-query_len)/4.0;
    else
        ratio = 0.2;
    ratio = 0.4;
    cout << "Option C:" << endl;
    for (int k=0; k<query_len; k++) // each time, deal with one term
    {
        dwtime_sort = 0.0;
        vector<V_Occurence> current_posting; // doc posting for current term
        current_posting.clear();
	bool aa = MakeChoice(k);
	gettimeofday(&dwstart, NULL);
        int f = search_frag[k].size();
        flag_do_optionA = f < (y*ratio);
        if(aa)
        {
            // Option A:
            // Step 1: Convert the fragment posting to document posting.
            //vector<V_Occurence> current_posting; // doc posting for current term
            //current_posting.clear();

            // algorithm for step 1: insert -> sort -> merge, this can merge the same vids
            // insert: insert each posting, the problem is that for different fragments, they might match the same vid, so that there will be multiple entries for the same vid. This duplication will be merged later.
            vector<Occurence> *vector_occ = &(search_frag[k]);
            for (int j=0; j<vector_occ->size(); j++) // each time, deal with one fragment
            {
                Occurence *o = &((*vector_occ)[j]); // the fragment occurence object
                int fid = o->fid; // fid
                vector<ReuseTableInfo> *vector_reuse = &(frag_reuse_table[fid]); // fragment reuse entry for fid
                for (int m=0; m<vector_reuse->size(); m++) // each time deal with a reuse version page for a given fragment
                {
                    int vid = (*vector_reuse)[m].vid;
                    int frag_offset = (*vector_reuse)[m].offset; // fragment's offset in the page
                    V_Occurence element;
                    current_posting.push_back(element); // firstly push back an empty element
                    V_Occurence *v_occur = &(current_posting[current_posting.size()-1]); // get the element just pushed in
                    v_occur->vid = vid;
                    v_occur->pos.clear();
                    for (int l=0; l<o->v_pos.size(); l++)
                        v_occur->pos.push_back(frag_offset+o->v_pos[l]);
                }
            }
            // sort: sort current_posting by its element's vid entry
            gettimeofday(&dwstart_sort, NULL);
            sort(current_posting.begin(), current_posting.end(), mycmpA);
            //gettimeofday(&dwend_sort, NULL);
            //dwtime_sort = 1000.0*(dwend_sort.tv_sec-dwstart_sort.tv_sec)+(dwend_sort.tv_usec-dwstart_sort.tv_usec)/1000.0;
            // merge: After sorting, elements with same vid must be adjacent. Merge them.
            vector<V_Occurence> *merge_result = &(doc_posting[k]);
            merge_result->clear();
            merge_result->push_back(current_posting[0]); // insert first element
            int pre_index = 0;
            for (int j=1; j<current_posting.size(); j++)
            {
                if (current_posting[j].vid != (*merge_result)[pre_index].vid){
                    merge_result->push_back(current_posting[j]);
                    pre_index++;
                }
                else // merge
                {
                    vector<int>::iterator iter;
                    for (iter = current_posting[j].pos.begin(); iter != current_posting[j].pos.end(); iter++)
                        (*merge_result)[pre_index].pos.push_back(*iter);
                }
            }


            vector<V_Occurence> *current = &(doc_posting[k]);
            for (int j=0; j<current->size(); j++)
            {
                int v = (*current)[j].vid;
                set<int> *tmp = new set<int>;
                tmp->clear();
                if (intersection_hash.find(v)==intersection_hash.end()){
                    vector<set<int>> new_entry; new_entry.clear();
                    intersection_hash[v]=new_entry;
                }
                intersection_hash[v].push_back(*tmp);
                vector<int> *p = &((*current)[j].pos);
                vector<int>::iterator iter;
                for (iter = p->begin(); iter != p->end(); iter++){
                    intersection_hash[v][k].insert(*iter);
                }
            }

	        gettimeofday(&dwend_sort, NULL);
            dwtime_sort = 1000.0*(dwend_sort.tv_sec-dwstart_sort.tv_sec)+(dwend_sort.tv_usec-dwstart_sort.tv_usec)/1000.0;
        }
        else
        {
            // Option C:
            vector<Occurence> *term_posting = &(search_frag[k]);
            //y=0; // avg number of frags in vid_list
            for (int i=0; i<vid_list.size(); i++) // each time, deal with one vid
            {
                vid = vid_list[i];
	
                //y += forward_table[vid].size();
                //vector<vector<int>> *e = new vector<vector<int>>(query_len);
                for (int j=0; j<forward_table[vid].size(); j++) // each time, deal with one fid in the vid
                {
                    fid = forward_table[vid][j].fid;
                    frag_offset = forward_table[vid][j].offset;
                    // for each fid, lookup the sorted term-fid postings
                    // binary search fid in term_posting O(log(n))
                    int index = find_index(term_posting, fid, 0, term_posting->size()-1);
                    if (index != -1){
                        vector<int>* vpos = &((*term_posting)[index].v_pos);
                        // for each position, insert it into vid_posting
                        for (int l=0; l<vpos->size(); l++)
                            (e[i])[k].push_back((*vpos)[l]+frag_offset);
                    }           
                }

                if (intersection_hash.find(vid)==intersection_hash.end()){
                    vector<set<int>> new_entry; new_entry.clear();
                    intersection_hash[vid]=new_entry;
                }
                set<int> *tmp = new set<int>;
                tmp->clear();
                intersection_hash[vid].push_back(*tmp);
                for (int x = 0; x < e[i][k].size(); x++){
                    intersection_hash[vid][k].insert(e[i][k][x]);
                }
	/*
                int j=0;
                int j1=0;
                int size1 = forward_table[vid].size();
                int size2 = term_posting->size();
                while(j<size1 && j1<size2){
                    if(forward_table[vid][j].fid == (*term_posting)[j1].fid){
                        frag_offset = forward_table[vid][j].offset;
                        vector<int>* vpos = &((*term_posting)[j1].v_pos);
                        for (int l=0; l<vpos->size(); l++)
                            (e[i])[k].push_back((*vpos)[l]+frag_offset);
                        j++;
                        j1++;
                    }
                    else if(forward_table[vid][j].fid < (*term_posting)[j1].fid)
                        j++;
                    else
                        j1++;
                }
	*/

            }
            //y /= vid_list.size();
        }
        gettimeofday(&dwend, NULL);
        dwtime = 1000.0*(dwend.tv_sec-dwstart.tv_sec)+(dwend.tv_usec-dwstart.tv_usec)/1000.0 -dwtime_sort;
        printf("%d %d\n",k,query_len);
        cout << "Calculate Term " << k << " : " << dwtime << " ms f= " << aa << endl;
    }
    cout << endl;

    // end OptionD algorithm

    // end timing
    //print_vid_postingC(); // validate results posting
    //print_vid_postingA(); // validate results posting
}




// calculate the min span in title: sliding window algorithm O(n)
void cal_min_span_title(int &min_span, int vid, vector<set<int>> &occur_terms, int term_number, int title_len)
{
    // cur_pos saves the current focus of terms' positions in the algorithm
    int *cur_pos = new int[term_number];
    int *cur_index = new int[term_number];
    if (term_number != occur_terms.size())
    {
        cout << "error in cal_min_span_title: term_number != occur_terms.size()" << endl;
        exit(1);
    }
    // copy occur_terms to vec_occ because set cannot be sorted
    vector<vector<int>> vec_occ(term_number);
    for (int i=0; i<term_number; i++)
    {
        vec_occ[i].clear();
        copy(occur_terms[i].begin(), occur_terms[i].end(), back_inserter(vec_occ[i]));
        sort(vec_occ[i].begin(), vec_occ[i].end()); // sort each term's positions from small to large
        cur_pos[i] = vec_occ[i][0]; // let cur_pos focus on the first occurence of each term
        cur_index[i] = 0;
    }
    // each time check if need to update min_span, move min_pos to next to check the next loop
    while(1)
    {
        int min, max, min_index, max_index;
        min = 99999, max = -99999;
        // every time, min is the smallest position among all current term position focus, max is the largest one
        for (int i=0; i<term_number; i++)
        {
            if (min > cur_pos[i]) {min=cur_pos[i]; min_index=i;}
            if (max < cur_pos[i]) {max=cur_pos[i]; max_index=i;}
        }
        // if max is not in title, terminate the loop
        if (max >= title_len)
            break;
        if (max - min + 1 < term_number) // double check: If true, something is wrong with our index.
        {
            cout << "error in cal_min_span: max-min+1<term_number" << endl;
            cout << "vid=" << vid << " min=" << min << " max=" << max << endl;
            exit(1);
        }

        if (min_span > max - min + 1) // check if we need to update min_span
            min_span = max - min + 1;
        // termination condition is cur_index[min_index] is the last one
        if (cur_index[min_index] == vec_occ[min_index].size() - 1)
            break;
        // otherwise, find the next one
        cur_index[min_index]++;
        cur_pos[min_index] = vec_occ[min_index][cur_index[min_index]];
    }
    delete []cur_pos;
    delete []cur_index;
}

// calculate the min span in body: sliding window algorithm O(n)
void cal_min_span(int &min_span, int vid, vector<set<int>> &occur_terms, int term_number)
{
    // cur_pos saves the current focus of terms' positions in the algorithm
    int *cur_pos = new int[term_number];
    int *cur_index = new int[term_number];
    if (term_number != occur_terms.size())
    {
        cout << "error in cal_min_span: term_number != occur_terms.size()" << endl;
        exit(1);
    }
    // copy occur_terms to vec_occ because set cannot be sorted
    vector<vector<int>> vec_occ(term_number);
    for (int i=0; i<term_number; i++)
    {
        vec_occ[i].clear();
        copy(occur_terms[i].begin(), occur_terms[i].end(), back_inserter(vec_occ[i]));
        sort(vec_occ[i].begin(), vec_occ[i].end()); // sort each term's positions from small to large
        cur_pos[i] = vec_occ[i][0]; // let cur_pos focus on the first occurence of each term
        cur_index[i] = 0;
    }
    // each time check if need to update min_span, move min_pos to next to check the next loop
    while(1)
    {
        int min, max, min_index, max_index;
        min = 99999, max = -99999;
        for (int i=0; i<term_number; i++)
        {
            if (min > cur_pos[i]) {min=cur_pos[i]; min_index=i;}
            if (max < cur_pos[i]) {max=cur_pos[i]; max_index=i;}
        }
        if (max - min + 1 < term_number) // double check: If true, something is wrong with our index.
        {
            cout << "error in cal_min_span: max-min+1<term_number" << endl;
            cout << "vid=" << vid << " min=" << min << " max=" << max << endl;
            exit(1);
        }

        if (min_span > max - min + 1) // check if we need to update min_span
            min_span = max - min + 1;
        // termination condition is cur_index[min_index] is the last one
        if (cur_index[min_index] == vec_occ[min_index].size() - 1)
            break;
        // otherwise, find the next one
        cur_index[min_index]++;
        cur_pos[min_index] = vec_occ[min_index][cur_index[min_index]];
    }
    delete []cur_pos;
    delete []cur_index;
}

int body_tf(int vid, vector<set<int>> &occur_terms, int term_number)
{
    if (term_number != occur_terms.size())
    {
        cout << "error in body_tf: term_number != occur_terms.size()" << endl;
        exit(1);
    }
    int *tf = new int[term_number];
    int min_tf = 99999;
    for (int i=0; i<term_number; i++)
    {
        tf[i] = occur_terms[i].size();
        if (min_tf > tf[i])
            min_tf = tf[i];
    }
    return min_tf;
}

void score_page(int vid, vector<set<int>> &occur_terms)
{
    ScoreResult new_entry;
    new_entry.vid = vid;
    double score;
    // now we need to calculate the score for vid
    double title_score, body_score;
    double text_appear_score, proximity_score;
    int term_number = search_frag.size();
    
    // calculate for title:
    int title_len = vid_titlelen_hash[vid];
    int appear=0;
    int min_span=9999;
    for (int i=0; i<term_number; i++)
    {
        bool if_cal_appear = false; // flag
        set<int>::iterator iter_set;
        for (iter_set = occur_terms[i].begin(); iter_set != occur_terms[i].end(); iter_set++)
        {
            if (*iter_set < title_len && if_cal_appear == false)
            {
                appear++;
                if_cal_appear = true;
            }
        }
    }
    text_appear_score = (double)appear / term_number;
    if (appear != term_number) // if not all terms occur in title, no proximity_score
        proximity_score = 0;
    else
    {
        min_span=9999;
        cal_min_span_title(min_span, vid, occur_terms, term_number, title_len);
        if (min_span < term_number)
        {
            cout << "min_span < term_number for title" << " something is wrong for cal_min_span_title" << endl;
            exit(1);
        }
        proximity_score = (double)term_number/min_span;
    }
    title_score = 1.0/6.0*text_appear_score + 5.0/6.0*proximity_score;
    
    // calculate for body:
    
    // old text_appear_score = 1
    //text_appear_score = 1; // text_appear_score=1 because in body we ensure that all text appears
    
    // new text_appear_score = log2(TF+1), TF is the min of all TFs.
    text_appear_score = log2(1.0+body_tf(vid, occur_terms, term_number));

    min_span=9999;
    cal_min_span(min_span, vid, occur_terms, term_number);
    proximity_score = (double)term_number/min_span; 
    body_score = 1.0/6.0*text_appear_score+5.0/6.0*proximity_score;

    score = 5.0/6.0*title_score + 1.0/6.0*body_score;
    new_entry.score = score;

    score_result.push_back(new_entry);
}

bool my_sort_function(const ScoreResult &v1, const ScoreResult &v2) { return (v1.score > v2.score); }

void scoring()
{
    // go through the intersection_hash and score each entry
    score_result.clear();
    unordered_map<int, vector<set<int>>>::iterator iter;
    for (iter = intersection_hash.begin(); iter != intersection_hash.end(); iter++)
    {
        score_page(iter->first, iter->second);
    }
    // sort score_result
    sort(score_result.begin(), score_result.end(), my_sort_function);
    // added by Susen
    //duration += (clock() - start) / (double) CLOCKS_PER_SEC;  // added
    // added by Susen
    ofstream fout;
    fout.open("result.txt");
    for (int i=0; i<score_result.size(); i++)
        fout << score_result[i].vid << " " << score_result[i].score<< endl;
    fout.close();
}


int main(int argc, char** argv)
{
    if (argc !=2)
    {
        cout << "need one parameter for query_len" << endl;
        exit(1);
    }
    query_len=atoi(argv[1]);
    // read reuse table: fid -> a list of vids for Option A
    read_index();
    // read postings
    read_search_frag();
    // read vids for Option C
    read_vid();
    // read forward reuse table: vid -> a list of fids for Option C
    read_forward();
    // calculate the positional information for each vid
    get_positional_info();
    scoring();
    return 0;
}

