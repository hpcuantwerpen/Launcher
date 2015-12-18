#include "verify.h"
#include "throw_.h"

bool is_absolute_path( QString const& folder )
{
    if( folder.isEmpty() ) {
        return false;
    }
    bool is_absolute = (folder.at(0)=='/');
#ifdef Q_OS_WIN
         is_absolute|= (folder.at(1)==':');
#endif
    return is_absolute;
}

bool verify_directory
  ( QDir* dir
  , bool  create_path
  , int*  error_code
  )
{
    if( error_code ) {
        *error_code = Ok;
    }
    bool ok = false;
    if( create_path ) {
        ok = dir->mkpath(QString());
        if( error_code && !ok ) {
            *error_code = CouldNotCreate;
        }
    } else {
        ok = dir->exists();
        if( error_code && !ok ) {
            *error_code = Inexisting;
        }
    }
    return ok;
}

bool verify_directory
  ( QDir*          dir
  , QString const& subfolder
  , bool           create_path
  , bool           stay_inside
  , QStringList*   decomposed
  , int*           error_code
  )
{
    if( error_code ) {
        *error_code = Ok;
    }
    if( subfolder.isEmpty() ) {
        return verify_directory(dir,create_path,error_code);
    }
    QString sdir = dir->absolutePath();
    QString relative_subfolder = dir->relativeFilePath(subfolder);
    bool is_inside = !relative_subfolder.startsWith("../");
    bool ok = false;
    if( (stay_inside && is_inside)
     || !stay_inside
      ) {
        if( create_path ) {
            if( dir->mkpath(relative_subfolder) ) {
                ok = dir->cd(relative_subfolder);
                if( error_code && !ok ) {
                    *error_code = CouldNotCreate;
                }
            }
        } else {
            ok = dir->cd(relative_subfolder);
            if( error_code && !ok ) {
                *error_code = Inexisting;
            }
        }
    } else {
        if( error_code && !ok ) {
            *error_code = Outside;
        }
    }
    if( ok && decomposed ) {
        decomposed->clear();
        decomposed->append(sdir);
        QStringList folders = relative_subfolder.split('/');
        QString job_folder = folders.last();
        folders.pop_back();
        decomposed->append( folders.join('/') );
        decomposed->append( job_folder );
    }
    return ok;
}

bool verify_file( QDir const* dir, QString const& file )
{
    if( file.contains('/') ) {
        throw_<std::runtime_error>("Invalid filename '%1'", file);
    }
    bool ok = dir->exists(file);
    return ok;
}
