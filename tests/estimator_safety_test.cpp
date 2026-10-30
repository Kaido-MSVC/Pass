#define PASS_ESTIMATOR_TESTING
#include "../src/strength_estimator.cpp"
#include <QCoreApplication>
#include <iostream>
#include <random>
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    int faults=0;
    for(const auto& input:{QString("Tr0ub4dor&3"),QString("correct horse battery staple"),QString(128,'a')}){
        allocationCalls=0;faultAfter=-1;
        pass::dictionaryScoreLimit(input);
        const int count=allocationCalls;
        if(liveAllocations || arena || !zeroVerified || count==0)return 1;
        // Exercise all allocation sites for short examples; sample the large
        // worst-case graph including its first and final allocations.
        for(int k=0;k<count;k+=(count>1000?count/100:1)){
            allocationCalls=0;faultAfter=k;
            if(pass::dictionaryScoreLimit(input)!=0)return 2;
            if(liveAllocations || arena || !zeroVerified)return 3;
            ++faults;
        }
        allocationCalls=0;faultAfter=count-1;
        if(pass::dictionaryScoreLimit(input)!=0 || liveAllocations || arena || !zeroVerified)return 4;
    }
    faultAfter=-1;
    // Exercise the instrumented implementation through its full UTF-8 boundary,
    // repeated graphs, Unicode, controls and varied lengths without saving text.
    std::mt19937 fixture(221);std::uniform_int_distribution<int> pick(1,0xffff);
    for(int n=0;n<2000;++n){
        QString input;for(int k=0;k<1+n%128;++k)input+=QChar(pick(fixture));
        const int score=pass::dictionaryScoreLimit(input);
        if(score<0 || score>100 || liveAllocations || arena || !zeroVerified)return 7;
        SecureZeroMemory(input.data(),static_cast<SIZE_T>(input.size())*sizeof(QChar));
    }
    if(pass::dictionaryScoreLimit(QString::fromLatin1("a\0b",3))!=0)return 5;
    if(pass::dictionaryScoreLimit(QString(129,'x'))!=0)return 6;
    std::cout<<"PASS estimator fault injection cases="<<faults<<" live="<<liveAllocations<<" wiped="<<wipedAllocations<<" zero_verified="<<zeroVerified<<"\n";
    return 0;
}
