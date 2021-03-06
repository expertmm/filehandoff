#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDir>
#include <QTextStream>
#include <QProcess>
#include <QTimer>
#include <QDebug>
#include <QStandardPaths>
#include <QtGlobal>  // qEnvironmentVariableIsSet etc
#include <pwd.h>
#include <grp.h>
#include <QMessageBox>

//see https://stackoverflow.com/questions/1009254/programmatically-getting-uid-and-gid-from-username-in-unix
gid_t MainWindow::getGroupIdByName(const char *name)
{
    struct group *grp = getgrnam(name); /* don't free, see getgrnam() for details */
    if(grp == NULL) {
        //throw runtime_error(std::string("Failed to get groupId from groupname : ") + name);
        throw std::runtime_error(std::string("Failed to get groupId from groupname : ") + name);
    }
    return grp->gr_gid;
}

//see https://stackoverflow.com/questions/1009254/programmatically-getting-uid-and-gid-from-username-in-unix
uid_t MainWindow::getUserIdByName(const char *name)
{
    struct passwd *pwd = getpwnam(name); /* don't free, see getpwnam() for details */
    if(pwd == NULL) {
        //throw runtime_error(std::string("Failed to get userId from username : ") + name);
        throw std::runtime_error(std::string("Failed to get userId from username : ") + name);
    }
    return pwd->pw_uid;
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->convert_ext["desktop"] = "desktop";
    this->convert_ext["pro"] = "pro";
    this->convert_ext["url"] = "url";
    this->convert_ext["fpp"] = "fpp";
    this->convert_ext["psd"] = "image/vnd.adobe.photoshop";
    this->convert_ext["jpg"] = "image/vnd.adobe.photoshop";
    this->convert_ext["jpeg"] = "image/vnd.adobe.photoshop";
    this->convert_ext["jpe"] = "image/vnd.adobe.photoshop";
    //TODO: this->convert_ext["lnk"] = "application/x-ms-shortcut";  // "Windows link"
    //this->convert_ext["mtl"] = "text/plain";
    //this->convert_ext["txt"] = "text/plain";
    //this->convert_ext["log"] = "text/plain";
    //NOTE: anything NOT registered above reverts to text/plain!
    this->editor_search_paths["text/plain"].append("/usr/bin/kate");
    this->editor_search_paths["text/plain"].append("/usr/bin/gedit");
    this->editor_search_paths["text/plain"].append("/usr/bin/leafpad");
    this->editor_search_paths["text/plain"].append("/usr/bin/mousepad");
    this->editor_search_paths["pro"].append("/usr/bin/qtcreator");
    this->editor_search_paths["pro"].append("/usr/local/bin/qtcreator");
    this->editor_search_paths["fpp"].append("/usr/bin/flashprint");
    this->editor_search_paths["fpp"].append("/usr/local/bin/flashprint");
    this->editor_search_paths["fpp"].append("/usr/share/FlashPrint/FlashPrint");  // poikilos' install script for Fedora results in FlashForge being here
    QTimer::singleShot(0, this, SLOT(handoff()));
    //handoff();
}

void MainWindow::handoff()
{
    //NOTE: arguments().at(0) is self
    int self_offset=1;
    bool url_enable=false;  //not really used--only useful for URL file syntax checking
    QString reconstructed_command;
    QProcessEnvironment environs = QProcessEnvironment::systemEnvironment();
    if (QCoreApplication::arguments().length()>self_offset) {
        if (QCoreApplication::arguments().length()==self_offset+1) {
            QString source_path = QCoreApplication::arguments().at(self_offset);
            QString source_path_original = source_path;
            if (source_path.toLower().startsWith("smb://") || source_path.startsWith("\\\\")) {
                int server_start = 6;
                QString slash = "/";
                if (source_path.startsWith("\\\\")) {
                    server_start = 2;
                    slash="\\";
                }
                int server_end = source_path.indexOf(slash, server_start);
                if (server_end > -1) {
                    int share_i = server_end + 1;
                    int share_end = source_path.indexOf(slash, share_i);
                    QString slash_sub_path = "";
                    if (share_end<=-1) share_end = source_path.length();
                    else {
                        slash_sub_path = source_path.mid(share_end);
                        if (slash=="\\") slash_sub_path = slash_sub_path.replace("\\", "/");
                    }
                    try {
                        QString share_name = source_path.mid(share_i, share_end-share_i).toLower();
                        //if (qEnvironmentVariableIsSet("UID")) {
                        //NOTE: UID environment variable doesn't work in this context (probably since GUI)
                        QString server_name = source_path.mid(server_start, server_end-server_start).toLower();
                        //QString uid_string = QString::fromLocal8Bit( qgetenv("UID") );
                        QString this_user_name = qgetenv("USER");
                        QString uid_string = QString::number( getUserIdByName(this_user_name.toStdString().c_str()) );

                        source_path = "/run/user/" + uid_string + "/gvfs/smb-share:server=" + server_name + ",share=" + share_name + slash_sub_path;
                        qDebug() << "source_path changed from \"" << source_path_original << " to " << source_path;
                        //}
                        //else qDebug() << "missing environment variable UID";
                    }
                    //see <https://stackoverflow.com/questions/4661883/qt-c-error-handling>
                    catch (std::exception &e) {
                        //qFatal("Error %s sending event %s to object %s (%s)",
                            //e.what(), typeid(*event).name(), qPrintable(receiver->objectName()),
                            //typeid(*receiver).name());
                        qFatal("Could not finish: %s sending event to object",
                               e.what());
                        qDebug() << "Could not finish: " << e.what();
                    } catch (...) {
                        //qFatal("Error <unknown> sending event %s to object %s (%s)",
                            //typeid(*event).name(), qPrintable(receiver->objectName()),
                            //typeid(*receiver).name());
                        qFatal("Could not finish <unknown> sending event to object");
                        qDebug() << "Could not finish for unknown reason";
                    }
                }
            }  // end if smb:// or \\

            int dot_i = source_path.lastIndexOf('.');
            this->handoff_type = "text/plain";
            if ((dot_i > 0) && (dot_i < (source_path.length() - 1))) {
                this->ext_string = source_path.mid(dot_i+1).toLower();
                if (this->convert_ext.contains(this->ext_string)) this->handoff_type = this->convert_ext[this->ext_string];
                //else this->handoff_type = this->ext_string;  // do NOT create unknown types--all cases below should have a corresponding entry in convert_ext. This allows defaulting to text/plain for unknown extension.
            }
            else this->ext_string = "";  //ok since filenames with no extension like "LICENSE" fall back to "text/plain" as per default above
            // elseif 0 then no extension (0 means is hidden file with no extension)
            //QMessageBox msgBox;
            //msgBox.setText("ext_string: " + this->ext_string + "\n" +
            //               "handoff_type: " + this->handoff_type + "\n");
            //msgBox.exec();

            if (this->handoff_type == "desktop") {
                //TODO: tidy this up--almost identical to url--should be vastly different if not an internet shortcut
                this->myfolder_name="filehandoff";
                QString homeLocation = QStandardPaths::locate(QStandardPaths::HomeLocation, QString(), QStandardPaths::LocateDirectory);
                QString apps_data_path =  QDir::cleanPath(homeLocation + QDir::separator() + ".config");
                if (!QDir(apps_data_path).exists()) QDir().mkdir(apps_data_path);
                this->mydata_path = QDir::cleanPath(apps_data_path + QDir::separator() + this->myfolder_name);
                if (!QDir(this->mydata_path).exists()) QDir().mkdir(this->mydata_path);

                this->browsers_list_path = QDir::cleanPath(this->mydata_path + QDir::separator() + "browsers.txt");

                this->path = "";

                QFile browsersFile(this->browsers_list_path);
                QString try_path;
                if (browsersFile.open(QIODevice::ReadOnly)) {
                   QTextStream in(&browsersFile);
                   while (!in.atEnd()) {
                      QString line = in.readLine();
                      line = line.trimmed();
                      if (line.length()>0) {
                          try_path = line;
                          QFileInfo try_file(try_path);
                          this->path = try_path;
                          if (try_file.exists()) break;
                      }
                   }
                   browsersFile.close();
                }
                else {
                    this->browser_search_paths.append("/usr/bin/waterfox");
                    this->browser_search_paths.append("/usr/bin/firefox-kde");
                    this->browser_search_paths.append("/usr/bin/firefox-nightly");
                    this->browser_search_paths.append("/usr/bin/firefox-developer");
                    this->browser_search_paths.append("/usr/bin/firefox-beta");
                    this->browser_search_paths.append("/usr/bin/firefox");
                    this->browser_search_paths.append("/usr/bin/chromium");
                    this->browser_search_paths.append("/usr/bin/google-chrome");
                    this->browser_search_paths.append("/usr/bin/icecat");
                    this->browser_search_paths.append("/usr/bin/firefox"); //firefox again here to write to browsers.txt if none above exist
                    for (int i = 0; i < this->browser_search_paths.size(); ++i) {
                        //cout << this->browser_search_paths.at(i).toLocal8Bit().constData() << endl;
                        try_path = this->browser_search_paths.at(i);
                        QFileInfo try_file(try_path);
                        this->path = try_path;
                        if (try_file.isFile()) break; //uses /usr/bin/firefox (or whatever is last above) if all else fails
                    }
                    QFile caFile(this->browsers_list_path);
                    caFile.open(QIODevice::WriteOnly | QIODevice::Text);
                    if(!caFile.isOpen()){
                        qDebug() << "ERROR unable to open" << this->browsers_list_path << "for output";
                    }
                    QTextStream outStream(&caFile);
                    outStream << this->path;
                    caFile.close();
                }

                //this->args.append(theoretical_path);
                QFile inputFile(source_path);
                if (inputFile.open(QIODevice::ReadOnly))
                {
                   QTextStream in(&inputFile);
                   while (!in.atEnd())
                   {
                      QString line = in.readLine();
                      //if (line.startsWith("[")) {
                      //    if (line.toLower().startsWith("[internetshortcut]")) url_enable=true;
                      //    else url_enable=false;
                      //}
                      QString this_opener = "url[$e]=";
                      if (line.toLower().startsWith(this_opener)) {
                          url_enable=true;
                          QString url_string = line.right(line.length()-this_opener.length()).trimmed();
                          if (this->path=="/usr/bin/firefox") {
                              //BROKEN even with 2 hyphens: this->args.append("-new-tab "+url_string);
                              //this->args.append("--new-tab");  //creates new WINDOW in Firefox Quantum
                              this->args.append(url_string);
                          }
                          else this->args.append(url_string);
                          break;
                      }
                   }
                   inputFile.close();
                }
            }
            else if (this->handoff_type == "url") {
                this->myfolder_name="filehandoff";
                QString homeLocation = QStandardPaths::locate(QStandardPaths::HomeLocation, QString(), QStandardPaths::LocateDirectory);
                QString apps_data_path =  QDir::cleanPath(homeLocation + QDir::separator() + ".config");
                if (!QDir(apps_data_path).exists()) QDir().mkdir(apps_data_path);
                this->mydata_path = QDir::cleanPath(apps_data_path + QDir::separator() + this->myfolder_name);
                if (!QDir(this->mydata_path).exists()) QDir().mkdir(this->mydata_path);

                this->browsers_list_path = QDir::cleanPath(this->mydata_path + QDir::separator() + "browsers.txt");

                this->path = "";

                QFile browsersFile(this->browsers_list_path);
                QString try_path;
                if (browsersFile.open(QIODevice::ReadOnly)) {
                   QTextStream in(&browsersFile);
                   while (!in.atEnd()) {
                      QString line = in.readLine();
                      line = line.trimmed();
                      if (line.length()>0) {
                          try_path = line;
                          QFileInfo try_file(try_path);
                          this->path = try_path;
                          if (try_file.exists()) break;
                      }
                   }
                   browsersFile.close();
                }
                else {
                    this->browser_search_paths.append("/usr/bin/waterfox");
                    this->browser_search_paths.append("/usr/bin/firefox-kde");
                    this->browser_search_paths.append("/usr/bin/firefox-nightly");
                    this->browser_search_paths.append("/usr/bin/firefox-developer");
                    this->browser_search_paths.append("/usr/bin/firefox-beta");
                    this->browser_search_paths.append("/usr/bin/firefox");
                    this->browser_search_paths.append("/usr/bin/chromium");
                    this->browser_search_paths.append("/usr/bin/google-chrome");
                    this->browser_search_paths.append("/usr/bin/icecat");
                    this->browser_search_paths.append("/usr/bin/firefox"); //firefox again here to write to browsers.txt if none above exist
                    for (int i = 0; i < this->browser_search_paths.size(); ++i) {
                        //cout << this->browser_search_paths.at(i).toLocal8Bit().constData() << endl;
                        try_path = this->browser_search_paths.at(i);
                        QFileInfo try_file(try_path);
                        this->path = try_path;
                        if (try_file.isFile()) break; //uses /usr/bin/firefox (or whatever is last above) if all else fails
                    }
                    QFile caFile(this->browsers_list_path);
                    caFile.open(QIODevice::WriteOnly | QIODevice::Text);
                    if(!caFile.isOpen()){
                        qDebug() << "ERROR unable to open" << this->browsers_list_path << "for output";
                    }
                    QTextStream outStream(&caFile);
                    outStream << this->path;
                    caFile.close();
                }

                //this->args.append(theoretical_path);
                QFile inputFile(source_path);
                if (inputFile.open(QIODevice::ReadOnly))
                {
                   QTextStream in(&inputFile);
                   while (!in.atEnd())
                   {
                      QString line = in.readLine();
                      QString this_opener = "url=";
                      if (line.startsWith("[")) {
                          if (line.toLower().startsWith("[internetshortcut]")) url_enable=true;
                          else url_enable=false;
                      }
                      else if (line.toLower().startsWith(this_opener)) {
                          QString url_string = line.right(line.length()-this_opener.length()).trimmed();
                          if (this->path=="/usr/bin/firefox") {
                              //BROKEN even with 2 hyphens: this->args.append("-new-tab "+url_string);
                              //this->args.append("--new-tab");  //creates new WINDOW in Firefox Quantum
                              this->args.append(url_string);
                          }
                          else this->args.append(url_string);
                          break;
                      }
                   }
                   inputFile.close();
                }
            }
            else if (this->handoff_type == "image/vnd.adobe.photoshop") {
                //would be something like env WINEPREFIX="/home/owner/win32" /usr/bin/wine C:\\Program\ Files\\Adobe\\Photoshop\ Elements\ 5.0\\Photoshop\ Elements\ 5.0.exe
                //unless you have correctly edited ~/.bashrc and added something like:
                //export WINEPREFIX=/home/owner/win32
                //export WINEARCH=win32

                QString wine_prefix = QDir::home().filePath(".wine");
                if (QDir("Folder").exists(QDir::home().filePath("win32"))) {
                    wine_prefix = QDir::home().filePath("win32");
                }
                environs.insert("WINEARCH","win32");
                environs.insert("WINEPREFIX",wine_prefix);

                this->handoff_type="psd";

                //this->path="/usr/bin/wine";
                this->path="wine";
                reconstructed_command="env";
                reconstructed_command+=" WINEARCH=win32";
                reconstructed_command+=" WINEPREFIX="+wine_prefix;

                //NOTE: reconstructed_command is not needed (neither does it work).
                //see also:
                //* http://stackoverflow.com/questions/4265946/set-environment-variables-for-startdetached-qprocess#4267306
                //  which (suggests putenv and) references:
                //  * http://bugreports.qt-project.org/browse/QTBUG-2284
                //  * http://code.qt.io/cgit/qt/qtbase.git/tree/src/corelib/io?h=5.5
                //  which references:
                //  * https://bugreports.qt.io/browse/QTBUG-26343
                //    which gives code similar to code below as a workaround

                //this->path="env";
                //this->args.append("WINEARCH=win32");
                //this->args.append("WINEPREFIX="+wine_prefix);

                //this->args.append("C:\\Program\ Files\\Adobe\\Photoshop\ Elements\ 5.0\\Photoshop\ Elements\ 5.0.exe");
                this->args.append("C:\\Program Files\\Adobe\\Photoshop Elements 5.0\\Photoshop Elements 5.0.exe");
                QString root_winpath = "Z:";
                QString dest_path = root_winpath + source_path.replace("/","\\");
                this->args.append(dest_path);
                for (int i=0; i<this->args.length(); i++) reconstructed_command+=" "+args.at(i);
            }
            else if (this->editor_search_paths.contains(this->handoff_type)) {
                QString try_path;
                for (int i = 0; i < this->editor_search_paths[this->handoff_type].size(); ++i) {
                    //cout << this->browser_search_paths.at(i).toLocal8Bit().constData() << endl;
                    try_path = this->editor_search_paths[this->handoff_type].at(i);
                    QFileInfo try_file(try_path);
                    this->path = try_path;
                    if (try_file.isFile()) break; //uses /usr/bin/firefox (or whatever is last above) if all else fails
                }
                this->args.append(source_path);
            }
            else {
                QMessageBox msgBox;
                msgBox.setText("INTERNAL ERROR: no case is programmed for handoff_type " +
                               this->handoff_type + "but the handoff type was incorrectly " +
                               "added to convert_ext or set manually.");
                msgBox.exec();
            }
        }
        else { //too many arguments, so dump them to the screen:
            for (int i=self_offset; i<QCoreApplication::arguments().length(); i++) {
               ui->listWidget->addItem(QCoreApplication::arguments().at(i));
            }
        }
    }

    if (this->path.length()>0) {
        //QCoreApplication.exit(0);
        if (this->path.endsWith("wine")) {
            ui->listWidget->addItem("[WINEPREFIX: "+qgetenv("WINEPREFIX")+"]");
            ui->listWidget->addItem("[WINEARCH: "+qgetenv("WINEARCH")+"]");
        }
        ui->listWidget->addItem("path:");
        ui->listWidget->addItem(this->path);
        ui->listWidget->addItem("args:");
        ui->listWidget->addItems(this->args);


        if (this->path.endsWith("wine")) {
            QProcess process;
            process.setProcessEnvironment(environs);
            QByteArray data;
            process.start(this->path, this->args);
            if(process.waitForStarted()) {
                while(process.waitForReadyRead())
                    data.append(process.readAll());
                QFile file("/tmp/filehandoff.txt");
                file.open(QIODevice::WriteOnly);
                file.write(data);
                file.close();

                this->close();  // QApplication::quit();
            }
        }
        else {
            QProcess::startDetached(this->path, this->args);

            this->close();  // QApplication::quit();
        }
        //QTimer::singleShot(0, this, SLOT(QApplication::quit()));
        //QTimer::singleShot(0, this, SLOT(this->close()));
        //exit(0);
        //qApp->exit(0);
    }
    else {
        ui->listWidget->addItem("Nothing to do (the path to the original file was not found).");
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
