#include "launchercomment.h"
#include<QRegularExpression>

namespace pbs
{//-----------------------------------------------------------------------------
    LauncherComment*
    LauncherComment::
    parse( QString const &line)
    {
        if( !line.startsWith(LauncherComment::prefix_) )
            return nullptr;
        else
            return create<LauncherComment>(line);
    }
 //-----------------------------------------------------------------------------
    QString LauncherComment::prefix_ = "#LAU";
 //-----------------------------------------------------------------------------
    LauncherComment::
    LauncherComment( QString const &line, int ordinate, types::Type type)
      : UserComment(line,ordinate,type)
    {}
 //-----------------------------------------------------------------------------
    void LauncherComment::init()
    {
        QRegularExpressionMatch m;
        QString pattern = LauncherComment::prefix_;
                pattern+= "(\\s+)(.+)";
        QRegularExpression re(pattern); // line pattern
        m = re.match( this->text_ );
        if( !m.hasMatch() )
            throw_<std::runtime_error>("Ill formed LauncherComment: %1",this->text_);
        this->value_ = m.captured((2));
        re.setPattern("(\\w+)\\s*=\\s*(.+)"); // parameter pattern
        m = re.match( this->value_ );
        if( !m.hasMatch() )
            throw_<std::runtime_error>("Ill formed LauncherComment: %1",this->text_);
        this->parameters()[m.captured(1)] = m.captured(2);
        Text::staticCompose(this);
    }
 //-----------------------------------------------------------------------------
    void LauncherComment::compose()
    {
        this->value_.clear();
        this->value_.append(this->parameters().first().first)
                    .append(" = ")
                    .append(this->parameters().first().second)
                    ;
        this->text_ = LauncherComment::prefix_
                    .append(" ")
                    .append(this->value_)
                    .append(this->parameters()[0].second)
                    ;
       Text::staticCompose(this);
    }
 //-----------------------------------------------------------------------------
    bool LauncherComment::
    equals( ShellCommand const* rhs) const
    {
        if( rhs->type() == types::LauncherComment ) {
            if( this->parameters().isEmpty() || rhs->parameters().isEmpty() )
                return false;
         // there's only one key and it should be the same
            return this->parameters().first().first == rhs->parameters().first().first;
        }
        return false;
    }
 //-----------------------------------------------------------------------------
    bool LauncherComment::copyFrom( ShellCommand const* rhs )
    {
        LauncherComment const* lc = dynamic_cast<LauncherComment const*>(rhs);
        for( Parameters_t::const_iterator iter = lc->parameters().cbegin(); iter != lc->parameters().cend(); ++iter)
        {
            QString const& key = iter->first;
            QString const& val = iter->second;
            if( this->parameters().index(key) == Parameters_t::not_found
             || this->parameters()[key] != val ) {
                this->parameters()[key] = val;
                this->set_is_modified();
            }
        }
        return this->is_modified();
    }
 //-----------------------------------------------------------------------------
}//namespace pbs

