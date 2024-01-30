using namespace std;

vector<string> split(const string& str, const char delim)
{
   vector<string> strings;
   size_t start;
   size_t end = 0;

   while ((start = str.find_first_not_of(delim, end)) != string::npos) {
      end = str.find(delim, start);
      strings.push_back(str.substr(start, end - start));
   }
   return strings;
}
