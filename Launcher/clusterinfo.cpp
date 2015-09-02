#include "clusterinfo.h"
#include <throw_.h>
#include <QVariant>
#include <QFile>
#include <QTextStream>
#include <cmath>

 //-----------------------------------------------------------------------------
 // NodesetInfo
 //-----------------------------------------------------------------------------
    NodesetInfo::
    NodesetInfo
      ( QString const& name
      , int     nNodes
      , int     nCoresPerNode
      , double  gbPerNode
      , double  gbPerNodeReserved
      , QStringList const& features
      , QStringList const& scriptActions
      , bool    isDefault
      )
      : name_(name)
      , nNodes_(nNodes)
      , nCoresPerNode_(nCoresPerNode)
      , gbPerNode_(gbPerNode)
      , gbPerNodeReserved_(gbPerNodeReserved)
      , features_(features)
      , scriptActions_(scriptActions)
      , isDefault_(isDefault)
    {}
 //-----------------------------------------------------------------------------
    NodesetInfo::NodesetInfo( NodesetInfo const& rhs )
      : name_(rhs.name_)
      , nNodes_(rhs.nNodes_)
      , nCoresPerNode_(rhs.nCoresPerNode_)
      , gbPerNode_(rhs.gbPerNode_)
      , gbPerNodeReserved_(rhs.gbPerNodeReserved_)
      , features_(rhs.features_)
      , scriptActions_(rhs.scriptActions_)
      , isDefault_(rhs.isDefault_)
    {}
 //-----------------------------------------------------------------------------
    NodesetInfo & NodesetInfo::operator=(  NodesetInfo const& rhs ) {
        name_ = rhs.name_;
        nNodes_ = rhs.nNodes_;
        nCoresPerNode_ = rhs.nCoresPerNode_;
        gbPerNode_ = rhs.gbPerNode_;
        gbPerNodeReserved_ = rhs.gbPerNodeReserved_;
        features_ = rhs.features_;
        scriptActions_ = rhs.scriptActions_;
        isDefault_ = rhs.isDefault_;
    }
 //-----------------------------------------------------------------------------
    void
    NodesetInfo::
    requestNodesAndCores( int nNodesRequested, int nCoresPerNodeRequested ) const
    {
        if( nNodesRequested > this->nNodes() ) {
            throw_<std::runtime_error>("Requesting more nodes than physically available (%1).", this->nNodes() );
        }
        if( nCoresPerNodeRequested > this->nCoresPerNode() ) {
            throw_<std::runtime_error>("Requesting more cores per node than physically available (%1).", this->nCoresPerNode() );
        }
        granted_.nCores        = nNodesRequested*nCoresPerNodeRequested;
        granted_.gbPerCore     = this->gbPerNodeAvailable()/nCoresPerNodeRequested;
        granted_.gbTotal       = nNodesRequested*this->gbPerNodeAvailable();
        granted_.nCoresPerNode = nCoresPerNodeRequested;
        granted_.nNodes        = nNodesRequested;
    }
 //-----------------------------------------------------------------------------
    void
    NodesetInfo::requestCoresAndMemory( int nCoresRequested, double gbPerCoreRequested ) const
    {
        if( gbPerCoreRequested > this->gbPerNodeAvailable() ) {
            throw_<std::runtime_error>("Requesting more memory per core than physically available (%1).", this->gbPerNodeAvailable() );
        }
        if( nCoresRequested > this->nCoresAvailable() ) {
            throw_<std::runtime_error>("Requesting more cores than physically available (%1).", this->nCoresAvailable() );
        }

        if( gbPerCoreRequested <= this->gbPerCoreAvailable() )
        {// use all cores per node since there is sufficient memory
            granted_.nCoresPerNode = this->nCoresPerNode();
            granted_.nNodes        = (int)( ceil( nCoresRequested / (double)(this->nCoresPerNode())) );
            if( granted_.nNodes==1 )
            {// respect the number of cores requested instead of returning a full node
                granted_.nCoresPerNode = nCoresRequested;
            }
        } else
        {//use only so many cores per nodes that each core has at least the requested memory
            granted_.nCoresPerNode = (int)( floor( this->gbPerNodeAvailable() / gbPerCoreRequested ));
            granted_.nNodes        = (int)( ceil( nCoresRequested / (double)(granted_.nCoresPerNode)));
        }
        granted_.nCores        = granted_.nNodes*granted_.nCoresPerNode;
        granted_.gbPerCore     = this->gbPerNodeAvailable() / granted_.nCoresPerNode;
        granted_.gbTotal       = granted_.nNodes*this->gbPerNodeAvailable();
        if( granted_.nNodes > this->nNodes() ) {
            throw_<std::runtime_error>("Requesting more nodes than physically available (%1).", this->nNodes() );
        }
    }
 //-----------------------------------------------------------------------------
 // ClusterInfoReader
 //-----------------------------------------------------------------------------
    ClusterInfoReader::
    ClusterInfoReader()
      : keyword_    ("^\\s*(\\w+)\\s*(.*)", QRegularExpression::DotMatchesEverythingOption )
      , list_begin_ ("^(\\[)\\s*(.*)"     , QRegularExpression::DotMatchesEverythingOption )
      , list_end_   ("^\\s*(\\])\\s*(.*)" , QRegularExpression::DotMatchesEverythingOption )
      , list_sep_   ("^\\s*(,)\\s*(.*)"   , QRegularExpression::DotMatchesEverythingOption )
      , word_       ("^([\\w*.-]+)(.*)"    , QRegularExpression::DotMatchesEverythingOption )
      , action_     ("^([+-]{[^}]*})(.*)" , QRegularExpression::DotMatchesEverythingOption )
      , number_     ("^(\\d+)(.*)"        , QRegularExpression::DotMatchesEverythingOption )
      , walltime_   ("^(\\d+[smhdw])(.*)" , QRegularExpression::DotMatchesEverythingOption )
      , clusterInfo_(nullptr)
    {}
 //-----------------------------------------------------------------------------
    void
    ClusterInfoReader::read( QString const& filename, ClusterInfo *clusterInfo)
    {
        this->rawClusterInfo_.clear();
        this->clusterInfo_ = clusterInfo;
        this->clusterInfo_->name_ = filename.left( filename.length()-5 );
        {
            QFile file( filename );
            file.open( QIODevice::ReadOnly | QIODevice::Text );
            QTextStream ts( &file );
            this->remainder_ = ts.readAll();
        }
        while( !this->remainder_.isEmpty() ) {
            this->readEntry_();
        }
        {// nodesets
            List_t const& list = this->rawClusterInfo_["nodesets"];
            for( int i=0; i<list.size(); ++i)
            {
                List_t const& list_i = list[i].toList();
                QString name( list_i.at(0).toString() );
                bool isDefault = false;
                if( name.startsWith('*') ) {
                    name = name.mid(1);
                    isDefault = true;
                }
                name.append(" (%1 Gb/node; %2 cores/node)");
                double gb = list_i.at(3).toDouble() - list_i.at(4).toDouble();
                NodesetInfo nodesetInfo
                  ( name.arg(gb).arg(list_i.at(2).toInt())
                  , list_i.at(1).toInt()
                  , list_i.at(2).toInt()
                  , list_i.at(3).toDouble()
                  , list_i.at(4).toDouble()
                  , list_i.at(5).toStringList()
                  , list_i.at(6).toStringList()
                  , isDefault
                  );
                this->clusterInfo_->nodesets_[nodesetInfo.name()] = nodesetInfo;
                if( nodesetInfo.isDefault() ) {
                    this->clusterInfo_->defaultNodeset_ = nodesetInfo.name();
                }
            }
        }{//login nodes
            List_t const& list = this->rawClusterInfo_["login_nodes"];
            for( int i=0; i<list.size(); ++i)             {
                this->clusterInfo_->login_nodes_.append( list.at(i).toString() );
            }
        }{
            List_t const& list = this->rawClusterInfo_["walltime_limit"];
            QString walltime_limit = list.at(0).toString();
            QChar unit = walltime_limit.right(1).at(0);
            int   num  = walltime_limit.left(walltime_limit.size()-1).toInt();
            int s = 1;
            if( unit=='s' ) {}
            else
            {   s*=60;
                if( unit=='m' ) {}
                else
                {   s*=60;
                    if( unit=='h' ) {}
                    else
                    {   s*=24;
                        if( unit=='d') {}
                        else{ s*=7; }
                    }
                }
            }
            num*=s;
            this->clusterInfo_->walltime_limit_ = num;
        }
    }
 //-----------------------------------------------------------------------------
   void ClusterInfoReader::throw_invalid_format_() const {
       throw_<std::runtime_error>("Invalid file format: %1, at: \n>>>>%2/n"
                                 , this->clusterInfo_->name()
                                 , this->remainder_ );
   }
 //-----------------------------------------------------------------------------
    bool ClusterInfoReader::match_( QRegularExpression const& re, bool must_throw )
    {
        this->m_ = re.match( this->remainder_ );
        if( this->m_.hasMatch() ) {
           this->token_ = this->m_.captured(1);
           this->remainder_ = this->m_.captured(this->m_.lastCapturedIndex());
           return true;
        }
        this->token_.clear();
        if( must_throw )
           this->throw_invalid_format_();
        return false;
    }
 //-----------------------------------------------------------------------------
    void ClusterInfoReader::readEntry_()
    {
       this->match_( this->keyword_, true );
       this->rawClusterInfo_[this->token()] = Raw_t::mapped_type();
       this->readList_( this->rawClusterInfo_[this->token()] );
    }
 //-----------------------------------------------------------------------------
    void ClusterInfoReader::readList_(List_t & list , bool initialBracketAlreadyDone )
    {
        if( !initialBracketAlreadyDone ) {
           this->match_( this->list_begin_, true );
        }
        while( true )
        {// read an item
           if( this->match_( this->list_begin_ ) )
           {// the item is itself a list
               List_t new_list;
               this->readList_( new_list, true );
               list.append( QVariant(new_list) );
           }
           else if( this->match_(this->walltime_) )
           {// item is a walltime_limit
               QVariant word = this->token();
               list.append(word);
           }
           else if( this->match_(this->number_) )
           {//item is a number
            // (not that a number is also matched by a word, hence we must first test for a number)
               QVariant number = this->token().toInt();
               list.append(number);
           }
           else if( this->match_(this->action_) )
           {// item is a jobscript action
               QVariant action = this->token();
               list.append(action);
           }
           else if( this->match_(this->word_) )
           {// item is a word
               QVariant word = this->token();
               list.append(word);
           }
        // read separator
           if( !this->match_(this->list_sep_) )
           {// read list end
               this->match_( this->list_end_, true );
               break;
           }
        }
    }
 //-----------------------------------------------------------------------------
 // ClusterInfo
 //-----------------------------------------------------------------------------
    ClusterInfo::ClusterInfo( QString const& filename )
    {
        ClusterInfoReader rdr;
        rdr.read(filename,this);
    }
 //-----------------------------------------------------------------------------
    ClusterInfo::ClusterInfo( ClusterInfo const& rhs )
      : name_(rhs.name_)
      ,  nodesets_(rhs.nodesets_)
      , login_nodes_(rhs.login_nodes_)
      , walltime_limit_(rhs.walltime_limit_)
      , defaultNodeset_(rhs.defaultNodeset_)
    {}
//-----------------------------------------------------------------------------
    ClusterInfo& ClusterInfo::operator=( ClusterInfo const& rhs )
    {
        name_ = rhs.name_;
        nodesets_ = rhs.nodesets_;
        login_nodes_ = rhs.login_nodes_;
        walltime_limit_ = rhs.walltime_limit_;
        defaultNodeset_ = rhs.defaultNodeset_;
    }

