#include <iostream>
#include <string>
using namespace std;

class Solution {
public:
    bool isMatch(string s, string p) {
        string temps;
        //size_t fs = 0;
        size_t ss, sp;
        ss = sp = 0;
        size_t fp = 0;
        int fg;
        char ct;

        while(ss != s.size()){
            int cd = 0;//".*...a" M "cba" -false;
            fg = 0;
            sp = fp;
            while(fp != p.size()){
                if(p[fp] == '*'){
                    fg = 1;
                    --fp;//fp can't be 0
                    ct = p[fp];
                    temps = p.substr(sp,fp-sp);
                    fp += 2;
                    while(p[fp] == ct || p[fp] == '*'){
                        ++ fp;
                        if(fp == p.size())
                            break;
                    }
                    break;
                }
                if(p[fp] == '.'){
                    fg = 1;
                    if(fp != (p.size()-1) && p[fp+1] == '*'){
                        ct = '*';
                        temps = p.substr(sp,fp-sp);
                        fp += 2;
                        while((fp != p.size() && (p[fp]=='.'))
                           || (fp != (p.size()-1) && (p[fp+1]=='*'))){
                            int df = 0;
                            if(p[fp] == '.'){
                                cd ++;
                                df = 1;
                            }
                            ++ fp;
                            if(fp == p.size())
                                return true;
                            if(p[fp] == '*'){
                                ++ fp;
                                if(df == 1)
                                    -- cd;
                            }
                        }
                        break;
                    }
                    --fp;
                    ct = '.';
                    temps = p.substr(sp,fp-sp+1);
                    fp += 2;
                    break;
                }
                ++ fp;
            }
            if(fp == p.size() && fg == 0)
                temps = p.substr(sp,fp-sp);
            //cout << temps.size() <<endl;
            for(size_t i = 0;i != temps.size();++ i)
                if(ss == s.size() || s[ss ++] != temps[i])
                    return false;
            if(ct == '.')
                ++ ss;
            else if(ct == '*'){
                //cout << p.size() << endl;
                if((ss+cd) >= s.size())
                    return false;
                ss += cd;
                if(fp == p.size())
                    return true;
                char tct1 = p[fp];
                char tct2;
                if(fp != (p.size()-1))
                    tct2 = p[fp+1];
                else{
                    if(s[s.size()-1] == p[fp])
                        return true;
                    else
                        return false;
                }

                while(ss != s.size()){//ss point to ccurent pos of s
                    if(s[ss] != tct1)
                            ++ ss;
                    else{
                        if((ss == (s.size()-1)))
                            return false;
                        else if(s[ss+1] != tct2)
                            ++ sp;
                        else if(isMatch(s.substr(ss,s.size()),
                                        p.substr(fp,p.size())))
                            return true;
                        else
                            ss += 2;
                    }
                }

                if(sp == p.size())
                    return true;
            }
            else
                while(ss != s.size() && s[ss] == ct)
                    ss ++;
            if(ss == s.size())
                if(fp != p.size())
                        return false;
                else
                    return true;
            else if(fp == p.size())
                return false;
            //fs = ss;
        }
        return true;
    }
};

int test_Solution(){
    string s1 = "acd";
    string s2 = ".*...d";
    Solution sltn;
    cout << sltn.isMatch(s1,s2) << endl;
    return 0;
}

