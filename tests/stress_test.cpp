#include "password.h"
#include "passphrase.h"
#include "wordlist.h"
#include "secret_text.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>
#include <QElapsedTimer>
#include <windows.h>
#include <psapi.h>
#include <stdexcept>

static bool characterOracle(const QString& text,const pass::Rules& r){
    const QString groups[]={"ABCDEFGHIJKLMNOPQRSTUVWXYZ","abcdefghijklmnopqrstuvwxyz","0123456789","!@#$%^&*()-_=+[]{};:,.?/"};
    int counts[4]{};
    if(text.size()!=r.length)return false;
    for(auto c:text){if(QStringLiteral("Il1O0o|").contains(c))return false;
        bool found=false;for(int i=0;i<4;++i)if(groups[i].contains(c)){++counts[i];found=true;}
        if(!found)return false;
    }
    for(auto count:counts)if(count<1)return false;
    return true;
}
static bool wordOracle(const QString& text,const pass::PassphraseRules& r){
    auto words=text.split(' ');
    struct Wiper{QStringList& words;~Wiper(){for(auto& word:words)wipeText(word);}}wipe{words};
    if(words.size()!=r.words)return false;
    for(const auto& word:words)if(!pass::wordlist().members.contains(word))return false;
    return true;
}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    if(argc!=2)return 2;
    QJsonArray runs;bool failed=false;
    for(auto algorithm:{pass::RandomAlgorithm::Ctr,pass::RandomAlgorithm::Hash,pass::RandomAlgorithm::Hmac,pass::RandomAlgorithm::Hmac512}){
        // Warm provider/wordlist caches before recording resource usage.
        {pass::Rules r;r.algorithm=algorithm;auto p=pass::generate(r);wipeText(p);}
        if(!pass::wordlist().available())return 3;
        DWORD handlesBefore=0,handlesAfter=0;GetProcessHandleCount(GetCurrentProcess(),&handlesBefore);
        PROCESS_MEMORY_COUNTERS_EX before{};before.cb=sizeof(before);GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&before),sizeof(before));
        QElapsedTimer timer;timer.start();int completed=0,characters=0,words=0;QString reason="limit";
        try{
            while(completed<1000000 && timer.elapsed()<180000){
                const auto cancel=[&]{return timer.elapsed()>=180000;};
                if(completed%2==0){pass::Rules r;r.algorithm=algorithm;r.length=completed%1000==0?128:20;
                    QString text=pass::generate(r,cancel);CandidateWiper wipe(text);
                    if(!characterOracle(text,r))throw std::logic_error("oracle");++characters;
                }else{pass::PassphraseRules r;r.algorithm=algorithm;r.words=completed%1000==1?12:6;r.separator=' ';
                    QString text=pass::generate(r,cancel);CandidateWiper wipe(text);
                    if(!wordOracle(text,r))throw std::logic_error("oracle");++words;
                }
                ++completed;
            }
            if(completed<1000000)reason="deadline";
        }catch(const std::logic_error&){reason="oracle_failure";failed=true;}
         catch(...){if(timer.elapsed()>=180000)reason="deadline";else{reason="generation_failure";failed=true;}}
        GetProcessHandleCount(GetCurrentProcess(),&handlesAfter);
        PROCESS_MEMORY_COUNTERS_EX after{};after.cb=sizeof(after);GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&after),sizeof(after));
        if(handlesAfter>handlesBefore+2){reason="handle_growth";failed=true;}
        runs.append(QJsonObject{{"algorithm",QString::fromLatin1(pass::algorithmId(algorithm))},{"completed",completed},{"character_passwords",characters},{"word_passphrases",words},{"elapsed_ms",static_cast<double>(timer.elapsed())},{"stop_reason",reason},{"handles_before",static_cast<int>(handlesBefore)},{"handles_after",static_cast<int>(handlesAfter)},{"private_bytes_before",static_cast<double>(before.PrivateUsage)},{"private_bytes_after",static_cast<double>(after.PrivateUsage)},{"peak_working_set_bytes",static_cast<double>(after.PeakWorkingSetSize)}});
    }
    // Only metadata: never serialize passwords, entropy samples or error contents.
    QSaveFile file(QString::fromLocal8Bit(argv[1]));if(!file.open(QIODevice::WriteOnly))return 4;
    file.write(QJsonDocument(QJsonObject{{"schema_version",1},{"max_per_algorithm",1000000},{"deadline_ms_per_algorithm",180000},{"passed",!failed},{"runs",runs}}).toJson());
    if(!file.commit())return 5;return failed?1:0;
}
