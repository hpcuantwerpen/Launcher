#ifndef CLUSTERINFO
#define CLUSTERINFO

#include <QString>
#include <QStringList>
#include <QMap>
#include <QRegularExpression>

 //=============================================================================
    class ClusterInfoReader;
 //=============================================================================
    class NodesetInfo
 //=============================================================================
    {
        friend class ClusterInfoReader;
    public:
        NodesetInfo() {}

        NodesetInfo
          ( QString const& name
          , int     nNodes
          , int     nCoresPerNode
          , double  gbPerNode
          , double  gbPerNodeReserved
          , QStringList const& features
          , QStringList const& scriptActions
          , bool    isDefault
          );
        NodesetInfo( NodesetInfo const& other );
        NodesetInfo& operator=( NodesetInfo const& rhs );

        struct Granted {
            int    nCores
                 , nNodes
                 , nCoresPerNode;
            double gbPerCore
                 , gbTotal;
        };

        void requestNodesAndCores ( int nNodes, int nCoresPerNode ) const;
         // find the result of the request using the granted() accessor
        void requestCoresAndMemory( int nCores, double gbPerCore=0. ) const;
         // find the result of the request using the granted() accessor
        Granted const& granted() const {
            return granted_;
        }

        double gbPerNodeAvailable() const {
            return gbPerNode_-gbPerNodeReserved_;
        }
        double gbPerCoreAvailable() const {
            return this->gbPerNodeAvailable()/this->nCoresPerNode();
        }
        QString const & name() const {
            return name_;
        }
        int nNodes() const {
            return nNodes_;
        }
        int nCoresPerNode() const {
            return nCoresPerNode_;
        }
        int nCoresAvailable() const {
            return nNodes()*nCoresPerNode();
        }
        QStringList const& features() const {
            return features_;
        }
        QStringList scriptActions() const {
            return scriptActions();
        }
        bool isDefault() const {
            return isDefault_;
        }
    private:
        QString name_;
        int     nNodes_;
        int     nCoresPerNode_;
        double  gbPerNode_;
        double  gbPerNodeReserved_;
        QStringList features_;
        QStringList scriptActions_;
        bool    isDefault_;
        mutable Granted granted_;
    };

 //=============================================================================
    class ClusterInfo
 //=============================================================================
    {
        friend class ClusterInfoReader;
    public:
        ClusterInfo() {}
        ClusterInfo( QString const& filename );
        ClusterInfo( ClusterInfo const& rhs );
        ClusterInfo& operator=( ClusterInfo const& rhs );

        QString const& name() const {
            return name_;
        }
        QMap<QString,NodesetInfo> const& nodesets() const {
            return nodesets_;
        }
        QString const& defaultNodeset() const {
            return defaultNodeset_;
        }

        typedef QMap<QString,NodesetInfo> Nodesets_t;

    private:
        QString name_;
        Nodesets_t nodesets_;
        QStringList login_nodes_;
        int walltime_limit_; //in seconds
        QString defaultNodeset_;
    };
 //=============================================================================
    class ClusterInfoReader
 //=============================================================================
    {
    public:
       ClusterInfoReader();
       void read(QString const& filename, ClusterInfo* clusterInfo );
    private:
       typedef QList<QVariant> List_t;
       void readEntry_();
       void readList_( List_t & list, bool initialBracketAlreadyDone=false );
       bool match_( QRegularExpression const& re, bool must_throw=false );

       QString const& token() const {
           return this->token_;
       }
       void throw_invalid_format_() const;
    private:
       QString remainder_;
       QString token_;
       QRegularExpressionMatch m_;
       QRegularExpression keyword_, list_begin_, list_end_, list_sep_, word_, action_, number_, walltime_;
       ClusterInfo* clusterInfo_;
       QRegularExpression *re_;
       typedef QMap<QString,List_t> Raw_t;
       Raw_t rawClusterInfo_;
    };
 //=============================================================================
#endif // CLUSTERINFO

