#include <iostream>
#include <vector>
#include <map>
#include <memory>
#include <cstring>
#include <cstdarg>
#include <curl/curl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef __linux
#define O_BINARY 0
#endif

using namespace std;

// useful for testing the code
#define USE_CACHE 0

// string with extended functions
//--------------------------------
struct corestring : public string {
//--------------------------------
    corestring() : string() {}
    corestring( const string &src ) : string( src ) {}
    corestring( const char *src ) : string( src ) {}
    corestring( const char src ) : string( &src, 1 ) {}
    void formatva( const char *format, va_list &arg_list ) {
        if( format ) {
            va_list cova;
            va_copy( cova, arg_list );
            int size = vsnprintf( NULL, 0, format, cova );
            va_end( arg_list );
            resize( size );
            va_copy( cova, arg_list );
            vsnprintf( &at( 0 ), size + 1, format, cova );
            va_end( arg_list );
        }
    }
    void format( const char *format, ... ) {
        if( format ) {
            va_list arg_list;
            va_start( arg_list, format );
            formatva( format, arg_list );
            va_end( arg_list );
        }
    }
    operator const char *() {
        return c_str();
    }
};

//--------------------------------
template <class V> struct corevector : public vector<V> {
//--------------------------------
    V &operator[]( size_t index ) {
        static V dummy;
        if( index < this->size() )
            return vector<V>::operator[]( index );
        return dummy;
    }
};

//--------------------------------
template <class I, class V> struct coremap : public map<I, V> {
//--------------------------------
    V &operator[]( I item ) {
        static V dummy;
        if( contains( item ))
            return map<I, V>::operator[]( item );
        return dummy;
    }
    bool contains( I item ) {
        return this->find( item ) != this->end();
    }
};

//--------------------------------
struct jsonItem {
//--------------------------------
    coremap<string, string> values;
    corevector<shared_ptr<jsonItem>> subItems;
    coremap<string, shared_ptr<jsonItem>> nodes;
    shared_ptr<jsonItem> operator = ( shared_ptr<jsonItem> other ) {
        values = other->values;
        subItems = other->subItems;
        nodes = other->nodes;
        return other;
    }
};

// simple but powerful json parser
//--------------------------------
class json {
//--------------------------------
    public:
        shared_ptr<jsonItem> main;
        json() {}
        void parse( const char *jsonText ) {
            vector<shared_ptr<jsonItem>> node;
            vector<string> text;
            vector<string> stack;
            string str;

            if( !jsonText )
                return;

            bool quote = false;
            bool escape = false;

            // splitting into nodes
            //--------------------------------
            for( auto ch = jsonText; *ch; ++ch ) {
                if( quote ) {
                    if( escape ) {
                        str += *ch;
                        escape = false;
                        continue;
                    } else if( '\\' == *ch ) {
                        escape = true;
                        continue;
                    }
                }
                if( '"' == *ch ) {
                    if( quote ) {
                        text.push_back( str );
                        str.clear();
                    }
                    quote = !quote;
                    continue;
                }
                if( quote ) {
                    str += *ch;
                    continue;
                }
                switch( *ch ) {
                    case '\n':
                    case ' ':
                    case '\t':
                        break;
                    case '[':
                    case ']':
                    case '{':
                    case '}':
                    case ',':
                    case ':':
                        if( str.length() )
                            text.push_back( str );
                        str.clear();
                        text.push_back( corestring( *ch ));
                        break;
                    default:
                        str += *ch;
                        break;
                }
            }

            // parsing the nodes and writing it into json structure
            //--------------------------------
            shared_ptr<jsonItem> curItem;
            if( text.size() || ( text.size() && text.front() != "[" )) {
                main = make_shared<jsonItem>();
                node.push_back( main );
                curItem = node.back();
            }
            string name, val, lastStack = "", lastNode = "";
            while( text.size() ) {
                if( stack.size() )
                    lastStack = stack.back();
                if( text.size() )
                    lastNode = text.front();
                if( "[" == lastNode ) {
                    stack.push_back( text.front() );
                    shared_ptr<jsonItem> newItem = make_shared<jsonItem>();
                    if( name.size() ) {
                        stack.push_back( "." );
                        shared_ptr<jsonItem> newItem = make_shared<jsonItem>();
                        curItem->nodes.insert(pair<string, shared_ptr<jsonItem> >( name, newItem ));
                        node.push_back( newItem );
                        curItem = node.back();
                        text.erase( text.begin() );
                        continue;
                    } else {
                        main = newItem;
                    }
                    node.push_back( newItem );
                    curItem = node.back();
                    text.erase( text.begin() );
                } else if( "]" == lastNode ) {
                    stack.erase( stack.end() );
                    node.erase( node.end() );
                    text.erase( text.begin() );
                    if( node.size() )
                        curItem = node.back();
                    else
                        break;
                } else if( "{" == lastNode ) {
                    if( "." == lastStack ) {
                        shared_ptr<jsonItem> newItem = make_shared<jsonItem>();
                        curItem->subItems.push_back( newItem );
                        node.push_back( newItem );
                        curItem = node.back();
                        stack.push_back( lastNode );
                        text.erase( text.begin() );
                        continue;
                    }
                    shared_ptr<jsonItem> newItem = make_shared<jsonItem>();
                    if( name.size() )
                        curItem->nodes.insert(pair<string, shared_ptr<jsonItem> >( name, newItem ));
                    else
                        curItem->subItems.push_back( newItem );
                    stack.push_back( lastNode );
                    node.push_back( newItem );
                    curItem = node.back();
                    text.erase( text.begin() );
                } else if( "," == lastNode ) {
                    text.erase( text.begin() );
                } else if( "}" == lastNode ) {
                    stack.erase( stack.end() );
                    node.erase( node.end() );
                    curItem = node.back();
                    text.erase( text.begin() );
                } else if( text.size() > 1 && text[1] == ":" ) {
                    name = text.front();
                    text.erase( text.begin() );
                    text.erase( text.begin() );
                    if( text.size() ) {
                        if( "[" == text.front() ||
                            "]" == text.front() ||
                            "{" == text.front() ||
                            "}" == text.front() )
                            continue;
                        val = text.front();
                        text.erase( text.begin() );
                        curItem->values.insert(pair<string, string>( name, val ));
                    }
                    name.clear();
                } else {
                    cout << "Parser error / invalid json source\n";
                }
            }
        }
} json;

//--------------------------------
size_t curlreaddata( void *contents, size_t size, size_t nmemb, string *str ) {
//--------------------------------
    size_t newLength = size * nmemb;
    str->append(( char * ) contents, newLength );
    return newLength;
}

//--------------------------------
void getstats( const char *user, const char *repo ) {
//--------------------------------
    string result;

#if USE_CACHE
    const char *filename = "curl.cache";
    int fd = ::open( filename, O_RDONLY | O_BINARY );
    if( fd > 0 ) {
        struct stat st;
        fstat( fd, &st );
        result.resize( st.st_size );
        size_t readBytes = ::read( fd, &result.front(), result.size() );
        close( fd );
        if( st.st_size != readBytes )
            return;
    } else {
#endif
        CURL *curl = curl_easy_init();
        corestring url;
        url.format( "https://api.github.com/repos/%s/%s/releases", user, repo );

        curl_easy_setopt( curl, CURLOPT_CUSTOMREQUEST, "GET" );
        curl_easy_setopt( curl, CURLOPT_URL, url.c_str() );

        curl_easy_setopt( curl, CURLOPT_USERAGENT, "libcurl-agent/1.0" );
        curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, curlreaddata );
        curl_easy_setopt( curl, CURLOPT_WRITEDATA, &result );

        curl_easy_perform( curl );
        curl_easy_cleanup( curl );
#if USE_CACHE
        fd = ::open( filename, O_CREAT | O_TRUNC | O_WRONLY | O_BINARY, 0666 );
        size_t writtenBytes = ::write( fd, &result.front(), result.size() );
        close( fd );
        if( result.size() != writtenBytes )
            return;
    }
#endif

    json.parse( result.c_str() );

    int totalDownloadCount = 0;
    for( auto item : json.main->subItems ) {
        cout << item->values[ "target_commitish" ] << " : " << item->values[ "name" ] << endl;
        if( item->nodes.contains( "assets" )) {
            auto downloaditems = item->nodes[ "assets" ]->subItems;
            for( auto download : downloaditems ) {
                cout << "\t" << download->values["name"] << " ( " << download->values["created_at"] << " ) # " << download->values["download_count"] << endl;
                totalDownloadCount += atoi( download->values["download_count"].c_str() );
            }
        }
    }
    cout << "-------------------\n";
    cout << "Total download count:" << totalDownloadCount << endl;
}

//--------------------------------
int main( int argc, char *argv[] ) {
//--------------------------------
    if( argc < 2 ) {
        cout << "Usage:\n" << argv[ 0 ] << " [github username] [github repo]\n";
        return 0;
    }
    getstats( argv[ 1 ], argv[ 2 ]);
    return 0;
}
//--------------------------------
