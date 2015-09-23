#ifndef TEXT_H
#define TEXT_H

#include <iostream>

#include <QString>

#define NOT_AVALAIBLE_FOR_THIS_TYPE(MEMBER) \
    throw_<std::logic_error>("Member #1 is not available for type %2",MEMBER,this->typeName());

namespace pbs
{//=============================================================================
    class Text
    {
    public:
        virtual ~Text();
        Text( QString const& txt, Text* parent=nullptr );
        virtual void init();
        virtual void compose() = 0;

        static void staticCompose(Text* _this_);

        QString const &text() const;

        bool is_modified() const;
        void set_is_modified( bool is_modified_ = true );
        void set_parent( Text* parent=nullptr );

        void set_has_unsaved_changes( bool has_unsaved_changes=true ) {
            this->has_unsaved_changes_ = has_unsaved_changes;
        }
        bool has_unsaved_changes() const {
            return has_unsaved_changes_;
        }
        virtual void print( std::ostream& to=std::cout, int verbose=0, bool refresh=false);

    protected:
        QString text_;
        QString body_;
        QString comment_; //trailing comment in the form "# blabla"
        Text* parent_;
        bool has_unsaved_changes_;
    };
//=============================================================================
}//namespace pbs

#endif // TEXT_H
