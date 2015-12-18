#ifndef VERIFY
#define VERIFY

#include <QString>
#include <QDir>

bool is_absolute_path(QString const& folder );

bool verify_directory( QDir* dir, bool create_path=false, int* error_code=nullptr );

bool verify_directory
  ( QDir*          dir
  , QString const& subfolder // test whether subfolder is an existing directory, if it is a
                             // relative path it is taken relative to dir.
  , bool create_path         // if subfolder is not existing, it is attempted to create it.
  , bool stay_inside=true    // requests that subfolder is in inside dir.
  , QStringList* decomposed=nullptr // local_file_location/project_folder/job_folder
  , int*         error_code=nullptr // a VerifyError value
  );
 /* If the operation is successful:
  *   . dir has cd'ed into subfolder,
  *   . subfolder is relative to the original dir
  */
bool verify_file( QDir const* dir, QString const& file );

enum VerifyError {
    Ok=0
  , Inexisting
  , Outside
  , CouldNotCreate
};

#endif // VERIFY

