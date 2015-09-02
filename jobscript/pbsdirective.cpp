#include "pbsdirective.h"
#include<QRegularExpression>

namespace pbs
{//-----------------------------------------------------------------------------
    PbsDirective*
    PbsDirective::
    parse( QString const &line)
    {
        if( !line.startsWith(PbsDirective::prefix_) )
            return nullptr;
        else
            return create<PbsDirective>(line);
    }
 //-----------------------------------------------------------------------------
    QString PbsDirective   ::prefix_ = "#PBS";
 //-----------------------------------------------------------------------------
    PbsDirective::
    PbsDirective( QString const &line, int ordinate, types::Type type)
      : UserComment(line,ordinate,type)
    {}
 //-----------------------------------------------------------------------------
    void PbsDirective::init()
    {
        QRegularExpressionMatch m;
        QRegularExpression re("#PBS\\s+(-\\w)\\s+([^\\s]*)(\\s*#.+)?");
        m = re.match( this->text_ );
        if( !m.hasMatch() )
            throw_<std::runtime_error>("Ill formed PbsDirective: %1",this->text_);
        this->flag_    = m.captured((1));
        this->value_   = m.captured((2));
        this->comment_ = m.captured((3));
        re.setPattern(":(\\w+)=((\\d{1,2}:\\d\\d:\\d\\d)|([\\w.-]+))(.*)");
        QString remainder = QString(':')+this->value_;
     // look for parameters
        m = re.match(remainder);
        if( m.hasMatch() ) {
            while( m.hasMatch() ) {
                this->parameters()[m.captured(1)] = m.captured(2);
                remainder = m.captured(m.lastCapturedIndex());
                m = re.match(remainder);
            }
         // look for features
            re.setPattern(":(\\w+)(.*)");
            m = re.match(remainder);
            while( m.hasMatch() ) {
                this->features().append(m.captured(0));
                remainder = m.captured(m.lastCapturedIndex());
                m = re.match(remainder);
            }
            if( !remainder.isEmpty() )
                throw_<std::runtime_error>("PBS line not fully matched, remainder='%1'", remainder );
        } else
        {// keep this->value_
        }
        Text::staticCompose(this);
    }
 //-----------------------------------------------------------------------------
    void PbsDirective::compose()
    {
        if( this->parameters().isEmpty() )
        {// keep value_
        } else {
            this->value_.clear();
            for( Parameters_t::const_iterator iter = this->parameters().cbegin(); iter != this->parameters().cend(); ++iter ) {
                this->value_.append(iter->first).append('=').append(iter->second).append(':');
            }
            for( Features_t::const_iterator iter= this->features().cbegin(); iter != this->features().cend(); ++iter ) {
                this->value_.append(*iter).append(':');
            }
            if( this->value_.endsWith(':') ) {
                this->value_.chop(1);
            }
        }
        this->text_ = QString("#PBS ")
                .append(this->flag_)
                .append(' ')
                .append(this->value_)
                .append(this->comment_)
                .append('\n')
                ;
    }
 //-----------------------------------------------------------------------------
    QString const&
    PbsDirective::
    value() const
    {
        if( !this->parameters().isEmpty()
         || !this->features().isEmpty() ) {
            throw std::runtime_error("Attempt to call PbsDirective::value() with non-empty parameters() and/or features().");
        }
        return this->value_;
    }
 //-----------------------------------------------------------------------------
    void
    PbsDirective::
    setValue( QString const& value )
    {
        if( !this->parameters().isEmpty()
         || !this->features().isEmpty() ) {
            throw std::runtime_error("Attempt to call PbsDirective::setValue() with non-empty parameters() and/or features().");
        }
        this->value_ = value;
    }
 //-----------------------------------------------------------------------------
    bool PbsDirective::
    equals( ShellCommand const* rhs) const
    {
        if( rhs->type() == types::PbsDirective ) {
            PbsDirective const* rhs2 = reinterpret_cast<PbsDirective const*>(rhs);
            if( this->flag_ != rhs2->flag_ ) // different flags are never equal
                return false;
            if( this->parameters().isEmpty() && rhs->parameters().isEmpty() ) // lhs and rhs have no parameters, hence equal
                return true;
         // otherwise they should have at least one common parameter
            for( toolbox::OrderedMap::const_iterator iter = this->parameters().cbegin(); iter!=this->parameters().cend(); ++iter )
                for( toolbox::OrderedMap::const_iterator itr2 = rhs->parameters().cbegin(); itr2!=rhs->parameters().cend(); ++itr2 )
                    if( iter->first == itr2->first )
                        return true;
        }
        return false;
    }
//-----------------------------------------------------------------------------
    bool PbsDirective::copyFrom( ShellCommand const* rhs )
    {
        PbsDirective const* pbs = dynamic_cast<PbsDirective const*>(rhs);
        if( this->parameters().isEmpty() ) {
            this->value_ = pbs->value_;
            this->set_is_modified();
        } else {
            for( Parameters_t::const_iterator iter = pbs->parameters().cbegin(); iter != pbs->parameters().cend(); ++iter)
            {
                QString const& key = iter->first;
                QString const& val = iter->second;
                if( this->parameters().index(key) == Parameters_t::not_found
                 || this->parameters()[key] != val ) {
                    this->parameters()[key] = val;
                    this->set_is_modified();
                }
            }
            for( Features_t::const_iterator iter = pbs->features().cbegin(); iter != pbs->features().cend(); ++iter)
            {
                QString const& feat = *iter;
                if( this->features().index(feat) == Features_t::not_found ) {
                    this->features().append(feat);
                    this->set_is_modified();
                }
            }
        }
        return this->is_modified();
    }
 //-----------------------------------------------------------------------------
}//namespace pbs

