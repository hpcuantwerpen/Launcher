#include "clusterinfo.h"
#include <throw_.h>
#include <QVariant>
#include <QFile>
#include <QTextStream>
#include <cmath>
#include <cassert>

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
    NodesetInfo & NodesetInfo::operator=( NodesetInfo const& rhs )
    {
        name_ = rhs.name_;
        nNodes_ = rhs.nNodes_;
        nCoresPerNode_ = rhs.nCoresPerNode_;
        gbPerNode_ = rhs.gbPerNode_;
        gbPerNodeReserved_ = rhs.gbPerNodeReserved_;
        features_ = rhs.features_;
        scriptActions_ = rhs.scriptActions_;
        isDefault_ = rhs.isDefault_;
        return *this;
    }
 //-----------------------------------------------------------------------------
    void NodesetInfo::storeResetValues( int nNodes, int nCoresPerNode, int nCores, double gbPerCore, double gbTotal ) const
    {
        this->resetValues_.nNodes        = nNodes;
        this->resetValues_.nCoresPerNode = nCoresPerNode;

        this->resetValues_.nCores        = nCores;
        this->resetValues_.gbPerCore     = gbPerCore;
        this->resetValues_.gbTotal       = gbTotal;
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
        resetValues_ = granted_;
    }
 //-----------------------------------------------------------------------------
    void
    NodesetInfo::requestCoresAndMemory(int nCoresRequested, double gbPerCoreRequested, IncreasePolicy increasePolicy ) const
    {
        if( gbPerCoreRequested > this->gbPerNodeAvailable() ) {
            throw_<std::runtime_error>("Requesting more memory per core than physically available (%1).", this->gbPerNodeAvailable() );
        }
        if( nCoresRequested > this->nCoresAvailable() ) {
            throw_<std::runtime_error>("Requesting more cores than physically available (%1).", this->nCoresAvailable() );
        }

        switch(increasePolicy) {
        case IncreaseCores:
        {// how many cores per node with this amount of memory?
            granted_.nCoresPerNode = int( floor( this->gbPerNodeAvailable() / gbPerCoreRequested ) );
         // how much memory per core, distributing the memory over the number of cores per node
            granted_.gbPerCore     = this->gbPerNodeAvailable()/granted_.nCoresPerNode;
         // how many nodes to have at least nCoresRequested in total
            int n_nodes, n_cores;
            for( n_nodes=1; n_nodes<=this->nNodes(); ++n_nodes ) {
                n_cores = n_nodes*granted_.nCoresPerNode;
                if( n_cores >= nCoresRequested )
                {//success
                    granted_.nNodes  = n_nodes;
                    granted_.gbTotal = n_nodes*this->gbPerNodeAvailable();
                    granted_.nCores  = n_cores;
                    resetValues_ = granted_;
                    return;
                }
            }
            throw_<std::runtime_error>("The request could not be satisfied.");
        }
        case IncreaseMemoryPerCore:
        {
            int n_nodes, n_cores_per_node;
            double gb_per_core;
            for( n_nodes=1; n_nodes<=this->nNodes(); ++n_nodes ) {
                n_cores_per_node = nCoresRequested / n_nodes;
                int remainder    = nCoresRequested % n_nodes;
                if( remainder==0 )
                {// how much memory do we have
                    gb_per_core = this->gbPerNodeAvailable() / n_cores_per_node;
                    if( gb_per_core >= gbPerCoreRequested )
                    {// success!
                        granted_.nCores        = nCoresRequested;
                        granted_.gbPerCore     = gb_per_core;
                        granted_.nNodes        = n_nodes;
                        granted_.gbTotal       = n_nodes * this->gbPerNodeAvailable();
                        granted_.nCoresPerNode = n_cores_per_node;
                        resetValues_ = granted_;
                        return;
                    }
                }
            }
            throw_<std::runtime_error>("The request cannot be satisfied with an equal number of cores per node on every node.");
        }
        default:
            throw_<std::logic_error>("IncreasePolicy not implemented.") ;
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
      , word_       ("^([\\w*.-]+)(.*)"   , QRegularExpression::DotMatchesEverythingOption )
      , action_     ("^([+-]{[^}]*})(.*)" , QRegularExpression::DotMatchesEverythingOption )
      , number_     ("^(\\d+)(.*)"        , QRegularExpression::DotMatchesEverythingOption )
      , walltime_   ("^(\\d+[smhdw])(.*)" , QRegularExpression::DotMatchesEverythingOption )
      , remote_cmd_ ("^\"(.*?)\"(.*)"          , QRegularExpression::DotMatchesEverythingOption )
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
                name.append(" (%1 GB/node; %2 cores/node)");
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
        }{//walltime_limit
            List_t const& list = this->rawClusterInfo_["walltime_limit"];
            QString walltime_limit = list.at(0).toString();
            QChar unit = walltime_limit.right(1).at(0);
            int   num  = walltime_limit.left(walltime_limit.size()-1).toInt();
            int s = nSecondsPerUnit(unit);
            num*=s;
            this->clusterInfo_->walltime_limit_ = num;
        }{//remote_commands
           List_t const& list = this->rawClusterInfo_["remote_commands"];
            assert(list.size()%2==0);
         // If there is a wrapper, wrap all default commands.
         // loop a first time over the list and apply the wrapper. Leave all
         // other entries alone.
            for ( List_t::const_iterator iter = list.cbegin()
                ; iter!=list.cend(); ++iter )
            {
                QString item = iter->toString();
                if( item=="wrapper" )
                {
                    QString wrapper = (++iter)->toString();
                 // wrap the default remote commands
                    for ( ClusterInfo::RemoteCommands_t::iterator itr2 = this->clusterInfo_->remote_commands_.begin()
                        ; itr2!=this->clusterInfo_->remote_commands_.end(); ++itr2  )
                    {
                        QString cmd = itr2.value();
                        cmd = wrapper.arg(cmd);
                        itr2.value() = cmd;
                    }
                 // insert the wrapper in the remote commands
                    this->clusterInfo_->remote_commands_["wrapper"] = wrapper;
                 // break out of the for loop
                    break;
                }
            }
         // loop over the list a second time to insert all the cluster's remote commands,
         // but skip the wrapper.
            for ( List_t::const_iterator iter = list.cbegin()
                ; iter!=list.cend(); ++iter )
            {
                QString key =    iter ->toString();
                QString cmd = (++iter)->toString();
                if( key!="wrapper" ) {
                    key.prepend("__"); // a prefix to distinguish keys from commands
                    this->clusterInfo_->remote_commands_[key] = cmd;
                }
            }
        }
    }
 //-----------------------------------------------------------------------------
    int nSecondsPerUnit( QChar const unit )
    {
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
                    else
                    {   s*=7;
                        if( unit!='w' ) {
                            throw_<std::runtime_error>("Unknown time unit : '%1'.", unit );
                        }
                    }
                }
            }
        }
        return s;
    }
    int nSecondsPerUnit( QString const& unit ) {
        return nSecondsPerUnit( unit.at(0) );
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
           else if( this->match_(this->remote_cmd_) )
           {// item is a remote command
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
        this->set_default_remote_commands();
        ClusterInfoReader rdr;
        rdr.read(filename,this);
        this->filename_ = filename;
    }
 //-----------------------------------------------------------------------------
    void ClusterInfo::set_default_remote_commands()
    {
        this->remote_commands_["__module_avail"] = "module -t av 2>&1";
    }
 //-----------------------------------------------------------------------------
    ClusterInfo::ClusterInfo( ClusterInfo const& rhs )
      : name_(rhs.name_)
      , nodesets_(rhs.nodesets_)
      , login_nodes_(rhs.login_nodes_)
      , walltime_limit_(rhs.walltime_limit_)
      , defaultNodeset_(rhs.defaultNodeset_)
      , remote_commands_(rhs.remote_commands_)
    {}
 //-----------------------------------------------------------------------------
    ClusterInfo& ClusterInfo::operator=( ClusterInfo const& rhs )
    {
        name_ = rhs.name_;
        nodesets_ = rhs.nodesets_;
        login_nodes_ = rhs.login_nodes_;
        walltime_limit_ = rhs.walltime_limit_;
        defaultNodeset_ = rhs.defaultNodeset_;
        remote_commands_ = rhs.remote_commands_;
        return *this;
    }
 //-----------------------------------------------------------------------------
//    std::map<std::string,std::string> ClusterInfo::std_remote_commands() const
//    {
//        std::map<std::string,std::string> std_map;
//        RemoteCommands_t::const_iterator iter = this->remote_commands_.constBegin();
//        while( iter != this->remote_commands_.constEnd()) {
//            std_map[iter.key().toStdString()] = iter.value();
//            ++iter;
//        }
//        return std_map;
//    }
