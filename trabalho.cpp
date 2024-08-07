#include <pthread.h>     
#include<unistd.h>  
#include <cstdio>
#include <cstdlib>
#include<iostream>
#include<stack>
#include<set>
#include <curl/curl.h>
#include<regex>
#include <fstream>

using namespace std;
using std::string;
//foi encontorado deadlock ou bloqueados???
#define N_THREADS 8
string kill = "=1";
int countlinks = 1;
pthread_mutex_t mutexcount;
pthread_mutex_t mutexlinks;
pthread_mutex_t mutextask;
pthread_mutex_t mutexwritefile;
set<string> links;
stack<string> tasks;
string baseurl = "https://www.ifb.edu.br";
int trabalhando = 0;
int contagemdelinks = 0;
pthread_mutex_t mutextrabalhando;
ofstream  arquivosaida;
ofstream  erros("erros.txt");

typedef struct Memoria {
    char* memory;
    size_t size;
} Memoria;

std::string urlDecode(const std::string& encoded) {
    int output_length;
    const auto decoded_value = curl_easy_unescape(nullptr, encoded.c_str(), static_cast<int>(encoded.length()), &output_length);
    std::string result(decoded_value, output_length);
    curl_free(decoded_value);
    return result;

}
std::string urlEncode(const std::string& decoded)
{
    const auto encoded_value = curl_easy_escape(nullptr, decoded.c_str(), static_cast<int>(decoded.length()));
    std::string result(encoded_value);
    curl_free(encoded_value);
    return result;
}
size_t WriteMemoryCallback (void* contents, size_t size, size_t nmemb, void* userp) {
    
    size_t realsize = size*nmemb;
    Memoria *mem = (Memoria *)userp;

    char* ptr = (char*) realloc(mem->memory, (mem->size+realsize +1)*(sizeof(char)));
    if (!ptr) {
        cout << "Memoria insuficiente" << endl;
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

void acessarLink (string site, string* resposta) {
    CURL *curl;
    CURLcode result;
    curl = curl_easy_init();
    if (curl == NULL) {
        return;
    }   
    Memoria memoria;
    memoria.memory = (char*) malloc(1);
    memoria.size = 0;
    char *link = &(site[0]);
    curl_easy_setopt(curl, CURLOPT_URL, link);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&memoria);
    

    result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        cout << "error " << site << endl;
        erros << result << endl;
        erros << "error" <<" " << site << endl;
        *resposta = memoria.memory;
        free(memoria.memory);
        curl_easy_cleanup(curl);
        return;
    }

    *resposta = memoria.memory;
    free(memoria.memory);
    curl_easy_cleanup(curl);

}

void *controlador(void *i){
    while(true){
        sleep(2);
        pthread_mutex_lock(&mutexcount);
        pthread_mutex_lock(&mutextrabalhando);
       
   
        if(trabalhando == 0 && countlinks ==0){
            pthread_mutex_lock(&mutextask);
            countlinks += 1;
            tasks.push(kill);
            pthread_mutex_unlock(&mutextask);
            pthread_mutex_unlock(&mutexcount); 
            pthread_mutex_unlock(&mutextrabalhando);   
            pthread_exit(NULL); 
        }
        pthread_mutex_unlock(&mutextrabalhando);
        pthread_mutex_unlock(&mutexcount);    
    }
}

void* operaria(void *id)
{
    //int i = *((int *) id) ;
    
    string html;
    string link;
    smatch m;
    smatch s;

    regex patternifblinkcompleto ("https?://www.ifb.edu.br[\\S]*\"");
    regex patternlinksisinifb ("^https?://www.ifb.edu.br[\\S]*");
    regex patterotherslinks("www.");
    regex patternlinkincompleto("href=\"/[\\S]*\"");
    regex patterremovecomerical("^(.*?)(?=&amp|$)");
    regex patternpdf("application/pdf");

    regex patteratags("<a[^>]*>");
    vector<string> filepatterns {"\\.png","\\.gif","\\.js","\\.css","\\.jpg","\\.JPG", "\\.jpeg", "\\.doc", "\\.docx","download"};
    string::const_iterator searchStart;

    while (true){
        // mudar para cond e fazer morrer quando acabar
            sleep(1);
            pthread_mutex_lock(&mutexcount);
            if(countlinks == 0){
                pthread_mutex_unlock(&mutexcount);
                continue;
            }
            pthread_mutex_lock(&mutextask);
            link = tasks.top();
            if(link == kill ){
                pthread_mutex_unlock(&mutextask);
                pthread_mutex_unlock(&mutexcount);
                pthread_exit(NULL); 
            }
            tasks.pop();
            pthread_mutex_lock(&mutextrabalhando);
            countlinks -= 1;
            contagemdelinks += 1;
            trabalhando += 1;
            cout << link << " " << trabalhando << " " << countlinks << " "  << contagemdelinks << endl;
    
     
            pthread_mutex_unlock(&mutextrabalhando);
            pthread_mutex_unlock(&mutextask);
            pthread_mutex_unlock(&mutexcount);
                
            acessarLink(link,&html);
            cout << link << " " << regex_search(html,patternpdf) << endl;
            if(regex_search(html,patternpdf)){
                pthread_mutex_lock(&mutexwritefile);

                arquivosaida << link << endl;
                pthread_mutex_unlock(&mutexwritefile);
                continue;
            }
            searchStart = html.cbegin();
            stack<string> templinks;
            while ( regex_search( searchStart, html.cend(), m, patteratags ) )
            {   
                string c = m[0];
                //cout << c << " test " << regex_search(c,patternlinkincompleto) << " " << regex_search(c,patternifblinkcompleto) << endl; 
                    
                if(regex_search( c.cbegin(), c.cend(), s, patternifblinkcompleto ) ){
                    c = s[0];
                    c = c.substr(0,c.length()-1);

                }
                else if(regex_search( c.cbegin(), c.cend(), s, patternlinkincompleto)){
                    c = s[0];
                    if(regex_search(c,patterotherslinks)){
                        searchStart = m.suffix().first;
                        continue;
                    }
                    c= baseurl + c.substr(6,c.length()-6-1);
                }
                else{
                    searchStart = m.suffix().first;
                    continue;
                }

                //c = urlDecode(c);
                searchStart = m.suffix().first;
                int flag =0;
                for(std::string pattern : filepatterns){
        
                    regex regexpattern(pattern);
                        if(regex_search(c,regexpattern)){
                  //          cout << c << endl;
                            flag = 1;
                            break;
                    }
                };
                if(flag || !regex_search(c,patternlinksisinifb)) {continue;}
                if (regex_search(c, m, patterremovecomerical)) {
                    // Acesse o resultado usando match[1]
                    c = m[1];
                }
                templinks.push(c);
            }
         
            pthread_mutex_lock(&mutexlinks);
            int novoslinks = 0;
            if(!templinks.empty()){
            pthread_mutex_lock(&mutextask);
            while(!templinks.empty()){
                string link = templinks.top();
                templinks.pop();
                if(links.find(link) != links.end()){
                    continue;
                };
                //cout << "link inserido " << link << endl; 
                novoslinks += 1;
                links.insert(link);
                tasks.push(link);
            }
            pthread_mutex_unlock(&mutextask);  

            }
            if(novoslinks){
                pthread_mutex_lock(&mutexcount);
                countlinks += novoslinks;
                pthread_mutex_unlock(&mutexcount);
            }
      
            pthread_mutex_unlock(&mutexlinks);
            pthread_mutex_lock(&mutextrabalhando);
            trabalhando -= 1;
            pthread_mutex_unlock(&mutextrabalhando);
    }
}


int main(int argc, char* argv[])
{
   if(argc != 2){
        cout << "uso ./executavel ArquivoDeSaida" << endl;
        return 1 ;
    }
    int i;
    arquivosaida.open(argv[1], ios::out);
    //arquivosaida.open("teste.txt",ios::out);
    pthread_t  thread_ids[N_THREADS]; 
    pthread_t controller;
    pthread_mutex_init(&mutextask,NULL);
    pthread_mutex_init(&mutexwritefile,NULL);
    pthread_mutex_init(&mutextrabalhando,NULL);
    pthread_mutex_init(&mutexcount,NULL);
    pthread_mutex_init(&mutexlinks,NULL);
    tasks.push("https://www.ifb.edu.br");
    links.insert("https://www.ifb.edu.br");
    i = -1;
    pthread_create(&controller, NULL, controlador,&i);
    for (i=0;i<N_THREADS; i++){
        pthread_create(&thread_ids[i], NULL, operaria, &i);     
    }
    for (int j=0;j<N_THREADS; j++){
        pthread_join(thread_ids[j], NULL);
    }
    
    printf("Fim!\n");
    pthread_exit(NULL); 
}
