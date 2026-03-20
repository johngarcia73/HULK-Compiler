bool issymbol(char& c)
{
    if ( c=='-' || c=='/' ||  c=='>' || c=='<' || c=='=' || c=='(' || c==')') return true;
    return false;
}
bool isEscapeSymbol(char& c)
{
    if (c == ('+') || c==('*')) return true;
    return false;
}

bool isEscapedUseless(char& c)
{
    if( c==('t') || c==('n')) return true;
    return false;
}