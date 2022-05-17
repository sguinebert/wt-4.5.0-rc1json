#include "WDropZone.h"

#include <Wt/WResource.h>
#include <Wt/WApplication.h>

#include "Wt/Http/Response.h"
#include "Wt/Json/json.hpp"
#include "Wt/Json/Parser.h"
#include "Wt/WApplication.h"
#include "Wt/WEnvironment.h"
#include "Wt/WMemoryResource.h"
#include "Wt/WServer.h"
#include "Wt/WSignal.h"

#include "WebUtils.h"
#include <exception>
#include <fstream>

namespace Wt {
class WDropZone::WDropZoneResource final : public WResource
{
public:
    WDropZoneResource(WDropZone *fileDropWidget, File *file)
        : WResource(),
        parent_(fileDropWidget)
        //, currentFile_(file)
    {
        setUploadProgress(true);
    }

    virtual ~WDropZoneResource()
    {
        beingDeleted();
    }

    //void setCurrentFile(File *file) { currentFile_ = file; }

protected:
    virtual void handleRequest(const Http::Request &request,
                               Http::Response &response) override
    {
        // In JWt we still have the update lock
#ifndef WT_TARGET_JAVA
        /**
     * Taking the update-lock (rather than posting to the event loop):
     *   - guarantee that the updates to WFileDropWidget happen immediately,
     *     before any application-code is called by the finished upload.
     *   - only Wt-code is executed within this lock
     */
      //  WApplication::UpdateLock lock(WApplication::instance());
#endif // WT_TARGET_JAVA

        // const std::string *fileId = request.getParameter("file-id");
        // if (fileId == 0 || (*fileId).empty()) {
        //     response.setStatus(404);
        //     return;
        // }
        // bool validId = parent_->incomingIdCheck(*fileId);
        // if (!validId) {
        //     response.setStatus(404);
        //     return;
        // }

        std::vector<std::string> uuids;
        std::vector<Http::UploadedFile> files;
        for(auto &[uuid, file] : request.uploadedFiles()) {
            //std::cout << "received uuid : " << uuid << std::endl;
            uuids.push_back(uuid);
            files.push_back(file);
        }
        
        //Utils::find(request.uploadedFiles(), "file", files);
        if (files.empty()) {
            response.setStatus(404);
            return;
        }

        // check is js filter was used
        //const std::string *filtFlag = request.getParameter("filtered");
        //currentFile_->setIsFiltered((filtFlag != 0) && ((*filtFlag) == "true"));
        

        for(unsigned i(0); i < uuids.size(); i++) {
            auto currentFile = parent_->incomingIdCheck(uuids[i]);
            if (!currentFile) {
                response.setStatus(404);
                return;
            }
            currentFile->handleIncomingData(files[i], false);
        }

        // add data to currentFile_
        // const std::string *lastFlag = request.getParameter("last");
        // bool isLast = (lastFlag == 0) || // if not present, assume not chunked
        //               (lastFlag != 0 && (*lastFlag) == "true");
        // currentFile_->handleIncomingData(files[0], isLast);

        // if (isLast) {
        //     parent_->proceedToNextFile();
        // }

        response.setMimeType("text/plain"); // else firefox complains
    }

private:
    WDropZone *parent_;
    //File *currentFile_;
};

WDropZone::WDropZone(const std::string& title) : WDropZone("", "", "", title)
{}

// WDropZone::WDropZone(const std::string& title, const std::string& mimetypes, bool uploadMultiple, unsigned filesize, unsigned maxfiles, bool disablePreviews) 
// : WDropZone("", "", "", title, mimetypes, uploadMultiple, filesize, maxfiles, disablePreviews)
// {}

WDropZone::WDropZone(const std::string& javaScriptFilter, 
                     const std::string& javaScriptTransform, 
                     const std::string& totaluploadprogress, 
                     const std::string& title, 
                     std::string mimetypes, 
                     bool uploadMultiple, 
                     unsigned filesize, 
                     unsigned maxfiles, 
                     bool disablePreviews)
    : uploadWorkerResource_(nullptr),
    resource_(nullptr),
    currentFileIdx_(0),
    maxfilesize_(filesize),
    maxfiles_(maxfiles),
    uploadMultiple_(uploadMultiple),
    jsFilterFn_(javaScriptFilter),
    jsTransformFn_(javaScriptTransform),
    chunkSize_(0),
    filterSupported_(true),
    hoverStyleClass_("Wt-dropzone-hover"),
    acceptDrops_(true),
    acceptAttributes_(mimetypes),
    title_(title),
    dropIndicationEnabled_(false),
    globalDropEnabled_(false),
    dropSignal_(this, "dropsignal"),
    requestSend_(this, "requestsend"),
    fileTooLarge_(this, "filetoolarge"),
    errorUpload_(this, "errorUpload"),
    uploadFinished_(this, "uploadfinished"),
    doneSending_(this, "donesending"),
    jsFilterNotSupported_(this, "filternotsupported"),
    updatesEnabled_(false),
    disablePreviews_(disablePreviews)
{
    WApplication *app = WApplication::instance();
    if (!app->environment().ajax())
        return;

    if(!jsFilterFn_.empty()) {
        jsFilterFn_.insert (0, "accept:");
        jsFilterFn_.push_back(',');
    }
    if(!jsTransformFn_.empty()){
        jsTransformFn_.insert (0, "transformFile:");
        jsTransformFn_.push_back(',');
    }
    if(!mimetypes.empty()){
        optionAcceptedFiles_ = "acceptedFiles:" + mimetypes + ",";
    }
    if(maxfilesize_ != 256) {
        optionfilesize_ = "maxFilesize:" + std::to_string(maxfilesize_) +",";
    }
    if(maxfiles_){
        optionmaxfiles_ = "maxFiles:" + std::to_string(maxfiles_) +",";
    }
    if(totaluploadprogress.empty()) {
        totaluploadprogress_ = totaluploadprogress;
    }

    setup();
}

WDropZone::~WDropZone() = default;


void WDropZone::enableAjax()
{
    setup();
    repaint();

    WContainerWidget::enableAjax();
}
void WDropZone::resetUrl()
{
    if(resource_) {
        doJavaScript(this->jsRef() + ".newurl('"+ resource_->generateUrl() +"');");
    }
}
void WDropZone::enable() {
    doJavaScript(this->jsRef() + ".enable();");
}

void WDropZone::disable() {
    doJavaScript(this->jsRef() + ".disable();");
}

void WDropZone::clearDropZone() {
    doJavaScript(this->jsRef() + ".clearDropZone();");
}

void WDropZone::setup()
{
    WApplication *app = WApplication::instance();

    app->useStyleSheet("resources/dropzone/basic.css");
    app->useStyleSheet("resources/dropzone/dropzone.css");
    app->require("resources/dropzone/dropzone-min.js"); 
    
    //LOAD_JAVASCRIPT(app, "js/WFileDropWidget.js", "WFileDropWidget", wtjs1);

//    std::string maxFileSize = std::to_string(WApplication::instance()->maximumRequestSize());
//    setJavaScriptMember(" WFileDropWidget", "new " WT_CLASS ".WFileDropWidget("
//                                                + app->javaScriptClass() + "," + jsRef() + ","
//                                                + maxFileSize + ");");

    resource_ = std::make_unique<WDropZoneResource>(this, nullptr);

    setJavaScriptMember(" WDropZone", "new Dropzone(" + this->jsRef() + ", { url: '"+ resource_->url() + "',"
                                                                            //"headers: {'file-id': 0},"
                                                                           + optionfilesize_ +
                                                                             optionmaxfiles_ +
                                                                             optionAcceptedFiles_ +
                                                                            "autoProcessQueue: false,"
                                                                            "uploadMultiple:" + std::to_string(uploadMultiple_) + ","
                                                                            "dictDefaultMessage: '" + title_ + "'," //Drop files here to upload
                                                                            "disablePreviews: "+ std::to_string(disablePreviews_) + ","
                                                                           + jsFilterFn_ +
                                                                            jsTransformFn_ +

                                                                            "init: function () {"
                                                                                  "var myDropzone = this;"
                                                                                  "myDropzone.options.paramName = function(n) {" //for multiple files per request
                                                                                        //"console.log('myDropzone.files[n].upload.uuid', n, myDropzone.files[n].upload.uuid);"
                                                                                        "return n;"
                                                                                  "};"
                                                                                  "Element.prototype.processFiles = function() {"
                                                                                        "myDropzone.processQueue();"
                                                                                  "};"
                                                                                  "Element.prototype.setFilter = function(fnFilter) {"
                                                                                      "myDropzone.options.accept = fnFilter;"
                                                                                  "};"
                                                                                  "Element.prototype.setMaxfileSize = function(maxsize) {"
                                                                                      "myDropzone.options.maxFilesize = maxsize;"
                                                                                  "};"
                                                                                  "Element.prototype.setMaxFiles = function(maxfiles) {"
                                                                                      "myDropzone.options.maxFiles = maxfiles;"
                                                                                  "};"
                                                                                  "Element.prototype.setAcceptedFiles = function(mime) {"
                                                                                      "myDropzone.options.acceptedFiles = mime;"
                                                                                  "};"
                                                                                  "Element.prototype.clearDropZone = function() {"
                                                                                      "myDropzone.removeAllFiles(true);"
                                                                                  "};" 
                                                                                  "Element.prototype.cancelUpload = function(uuid) {"
                                                                                      //"console.log(myDropzone.getRejectedFiles());"
                                                                                      "for (var i = 0; i < myDropzone.files.length; i++)"
                                                                                        "if(myDropzone.files[i].upload.uuid == uuid){"
                                                                                            "myDropzone.removeFile(myDropzone.files[i]);"
                                                                                            "return;"
                                                                                        "}"
                                                                                  "};"
                                                                                  "Element.prototype.newurl = function(url) {"
                                                                                      "myDropzone.options.url=url;"
                                                                                  "};" 
                                                                                  "Element.prototype.enable = function(url) {"
                                                                                      "myDropzone.enable();"
                                                                                  "};"
                                                                                  "Element.prototype.disable = function() {"
                                                                                      "myDropzone.disable();"
                                                                                  "};"
                                                                                  

//                                                                                  "transformFile: function transformFile(file, done) "{
//                                                                                        "zip = new JSZip();"
//                                                                                        "zip.file(file.name, file);"
//                                                                                        "zip.generateAsync("
//                                                                                               "{"
//                                                                                                   "type:'blob',"
//                                                                                                   "compression: 'DEFLATE'"
//                                                                                               "} ).then(function(content) {"
//                                                                                                "done(content);"
//                                                                                            "});"
//                                                                                   " },"
//                                                                                "this.on('drop', e => {"
//                                                                                     "console.log('drop event', e);"
//                                                                                    "var files = [];"
//                                                                                    "var infos = [];"
//                                                                                    "for (var i = 0; i < e.dataTransfer.files.length; i++) {"
//                                                                                        "files[i] = e.dataTransfer.files[i];"
//                                                                                        "console.log('file', files[i]);"
//                                                                                        "infos.push({'id':files[i].upload.uuid,'filename':files[i].name,'type':files[i].type,'size':files[i].size});"
//                                                                                    "}"
//                                                                                     "Wt.emit(" + this->jsRef() + ", 'dropsignal', JSON.stringify(infos));"
//                                                                                "});"

                                                                                "this.on('addedfile', file => {"
                                                                                    "Wt.emit(" + this->jsRef() + ", 'dropsignal', JSON.stringify([{'id':file.upload.uuid,'filename':file.name,'type':file.type,'size':file.size}]));"
                                                                                "});"
                                                                                "this.on('addedfiles', files => {"
//                                                                                    "var infos = [];"
//                                                                                    "for (var i = 0; i < files.length; i++) {"
//                                                                                         "if(files[i].upload)"
//                                                                                            "infos.push({'id':files[i].upload.uuid,'filename':files[i].name,'type':files[i].type,'size':files[i].size});"
//                                                                                    "}"
//                                                                                    "if(infos.length)"
//                                                                                         "Wt.emit(" + this->jsRef() + ", 'dropsignal', JSON.stringify(infos));"
                                                                                "});"
                                                                                "this.on('success', file => {"
                                                                                    "Wt.emit(" + this->jsRef() + ", 'uploadfinished', file.upload.uuid);"
                                                                                "});"
                                                                                "this.on('error', (file, message) => {"
                                                                                    "Wt.emit(" + this->jsRef() + ", 'errorUpload', file.upload.uuid, message);"
                                                                                "});"
                                                                                "this.on('maxfilesexceeded', file => {"
                                                                                    //"Wt.emit(" + this->jsRef() + ", 'filetoolarge', file.size);"
                                                                                "});"
                                                                                "this.on('canceled', file => {"
                                                                                    //"Wt.emit(" + this->jsRef() + ", 'filetoolarge', file.size);"
                                                                                "});"
                                                                                "this.on('complete', (file) => {"
                                                                                    //"Wt.emit(" + this->jsRef() + ", 'filetoolarge', file.size);"
                                                                                    "myDropzone.processQueue();"
                                                                                "});"
                                                                                "this.on('accept', (file, done) => {"
                                                                                    //"Wt.emit(" + this->jsRef() + ", 'filetoolarge', file.size);"
                                                                                "});"

                                                                                "this.on('queuecomplete', () => {"
                                                                                    //"Wt.emit(" + this->jsRef() + ", 'filetoolarge', file.size);"
                                                                                "});"
                                                                                "this.on('totaluploadprogress', (progress) => {"
                                                                                    //"Wt.emit(" + this->jsRef() + ", 'filetoolarge', file.size);"
                                                                                    //"console.log('totaluploadprogress', upprogress);"
                                                                                    // "var allProgress = 0;"
                                                                                    // "var allFilesBytes = 0;"
                                                                                    // "var allSentBytes = 0;"
                                                                                    // "for(var a=0;a<this.files.length;a++) {"
                                                                                    //     "allFilesBytes = allFilesBytes + this.files[a].size;"
                                                                                    //     "allSentBytes = allSentBytes + this.files[a].upload.bytesSent;"
                                                                                    //     "allProgress = (allSentBytes / allFilesBytes) * 100;"
                                                                                    // "}"
                                                                                    // "window.nanobar.go(allProgress);"
                                                        
                                                                                    //+ totaluploadprogress_ +
                                                                                "});"
                                                                                // "this.on('sending', function (file, xhr, formData) {"
                                                                                //     "formData.append('file-id', file.upload.uuid);"
                                                                                // "});"
                                                                            "}"
                                                                        "});");




    dropSignal_.connect(this, &WDropZone::handleDrop);
//    requestSend_.connect(this, &WDropZone::handleSendRequest);
    fileTooLarge_.connect(this, &WDropZone::handleTooLarge);
    errorUpload_.connect(this, &WDropZone::handleErrorUpload);
    uploadFinished_.connect(this, &WDropZone::emitUploaded);
//    doneSending_.connect(this, &WDropZone::stopReceiving);
//    jsFilterNotSupported_.connect(this, &WDropZone::disableJavaScriptFilter);

    addStyleClass("dropzone");
}

void WDropZone::handleDrop(const std::string& newDrops)
{
#ifndef WT_TARGET_JAVA
    Json::Array dropdata;
#else
    Json::Array& dropdata;
#endif
    Json::parse(newDrops, dropdata);

    std::vector<File*> drops;

    //Json::Array dropped = (Json::Array)dropdata;
    for (std::size_t i = 0; i < dropdata.size(); ++i) {
        Json::Object& upload = dropdata[i].as_object();
        std::string id;
        ::uint64_t size = 0;
        std::string name, type;
        for (auto &[key, value] : upload) {
#ifndef WT_TARGET_JAVA
            if (key == "id")
                id = value.get_string("");
            else if (key == "filename")
                name = value.get_string("");
            else if (key == "type")
                type = value.get_string("");
            else if (key == "size")
                size = value.get_int64(0);
#else
            if (key == "id")
                id = value.get_int64(-1);
            else if (key == "filename")
                name = (std::string)value.get_string("");
            else if (key == "type")
                type = (std::string)value.get_string("");
            else if (key == "size")
                size = value.get_int64(0);
#endif
            else
                throw std::exception();
        }

        File *file = new File(id, name, type, size, chunkSize_);
        drops.push_back(file);
        uploads_.push_back(file);
    }
    dropEvent_.emit(drops);
    doJavaScript(this->jsRef() + ".processFiles();"); //.item(0).processQueue();
}

void WDropZone::handleSendRequest(std::string& id)
{
    /* When something invalid is dropped, the upload can fail (eg. a folder).
   * We simply proceed to the next and consider this upload as 'cancelled'
   * since it is past the currentFileIdx.
   * A cancelled upload will also be skipped in this way.
   */

    return;


//    bool fileFound = false;
//    for (unsigned i = currentFileIdx_; i < uploads_.size(); i++) {
//        if (uploads_[i]->uploadId() == id) {
//            fileFound = true;
//            currentFileIdx_ = i;
//            delete resource_;
//            resource_ = new WDropZoneResource(this, uploads_[currentFileIdx_]);
//            resource_->dataReceived().connect(this, &WDropZone::onData);
//            resource_->dataExceeded().connect(this, &WDropZone::onDataExceeded);
//            doJavaScript(jsRef() + ".send('" + resource_->url() + "', "
//                         + (uploads_[i]->filterEnabled() ? "true" : "false")
//                         + ");");
//            uploadStart_.emit(uploads_[currentFileIdx_]);
//            break;
//        } else {
//            // If a previous upload was not cancelled, it must have failed
//            if (!uploads_[i]->cancelled())
//                uploadFailed_.emit(uploads_[i], "canceled");
//        }
//    }

//    if (!fileFound)
//        doJavaScript(jsRef() + ".cancelUpload(" + id + ");");
//    else {
        updatesEnabled_ = true;
        WApplication::instance()->enableUpdates(true);
//    }
}

void WDropZone::handleTooLarge(::uint64_t size)
{
    if (currentFileIdx_ >= uploads_.size()) {
        // This shouldn't happen, but a mischievous client might emit
        // this signal a few times, causing currentFileIdx_
        // to go out of bounds
        return;
    }
    tooLarge_.emit(uploads_[currentFileIdx_], size);
    currentFileIdx_++;
}

void WDropZone::handleErrorUpload(std::string uuid, std::string error)
{
    for (unsigned i = 0; i < uploads_.size(); i++) {
        if(uploads_[i]->uploadId() == uuid)
            uploadFailed_.emit(uploads_[i], error);
    }
}

void WDropZone::stopReceiving()
{
    if (currentFileIdx_ < uploads_.size()) {
        for (unsigned i=currentFileIdx_; i < uploads_.size(); i++)
            if (!uploads_[i]->cancelled())
                uploadFailed_.emit(uploads_[i], "stopped");
        // std::cerr << "ERROR: file upload was still listening, "
        // 	      << "cancelling expected uploads"
        // 	      << std::endl;
        currentFileIdx_ = uploads_.size();
        if (updatesEnabled_) {
            WApplication::instance()->enableUpdates(false);
            updatesEnabled_ = false;
        }
    }
}

// Note: args by value, since this is handled after handleRequest is finished
void WDropZone::proceedToNextFile()
{
    if (currentFileIdx_ >= uploads_.size()) {
        // This shouldn't happen, but a mischievous client might emit
        // the filetoolarge signal too many times, causing currentFileIdx_
        // to go out of bounds
        return;
    }

    currentFileIdx_++;
    if (currentFileIdx_ == uploads_.size()) {
        if (updatesEnabled_) {
            WApplication::instance()->enableUpdates(false);
            updatesEnabled_ = false;
        }
    }
}

void WDropZone::emitUploaded(std::string id)
{
    for (unsigned i=0; i < uploads_.size(); i++) { //i < currentFileIdx_ &&
        File *f = uploads_[i];
        if (f->uploadId() == id) {
            //f->uploaded().emit();
            uploaded_.emit(f);
        }
    }
}

Wt::WDropZone::File* WDropZone::incomingIdCheck(const std::string& id)
{
    for (unsigned i = 0; i < uploads_.size(); i++) {
        //std::cout << "incomingIdCheck : " <<  uploads_[i]->uploadId() << " - " << id << std::endl;
        if (uploads_[i]->uploadId() == id)
        {
            //resource_->setCurrentFile(uploads_[i]);
            return uploads_[i];
        }
    }
    return nullptr;
}

void WDropZone::cancelUpload(File *file)
{
    file->cancel();
    std::string i = file->uploadId();
    doJavaScript(jsRef() + ".cancelUpload(" + i + ");");
}

bool WDropZone::remove(File *file)
{
    for (unsigned i=0; i < currentFileIdx_ && i < uploads_.size(); i++) {
        if (uploads_[i] == file) {
            uploads_.erase(uploads_.begin()+i);
            currentFileIdx_--;
            return true;
        }
    }
    return false;
}

void WDropZone::onData(::uint64_t current, ::uint64_t total)
{
    if (currentFileIdx_ >= uploads_.size()) {
        // This shouldn't happen, but a mischievous client might emit
        // the filetoolarge signal too many times, causing currentFileIdx_
        // to go out of bounds
        return;
    }
    File *file = uploads_[currentFileIdx_];
    file->emitDataReceived(current, total, filterSupported_);

    WApplication::instance()->triggerUpdate();
}

void WDropZone::onDataExceeded(::uint64_t dataExceeded)
{
    if (currentFileIdx_ >= uploads_.size()) {
        // This shouldn't happen, but a mischievous client might emit
        // the filetoolarge signal too many times, causing currentFileIdx_
        // to go out of bounds
        return;
    }
    tooLarge_.emit(uploads_[currentFileIdx_], dataExceeded);

    WApplication *app = WApplication::instance();
    app->triggerUpdate();
}

void WDropZone::updateDom(DomElement& element, bool all)
{
    WApplication *app = WApplication::instance();
    if (app->environment().ajax()) {
//        if (updateFlags_.test(BIT_HOVERSTYLE_CHANGED) || all)
//            doJavaScript(jsRef() + ".configureHoverClass('" + hoverStyleClass_
//                         + "');");
//        if (updateFlags_.test(BIT_ACCEPTDROPS_CHANGED) || all)
//            doJavaScript(jsRef() + ".setAcceptDrops("
//                         + (acceptDrops_ ? "true" : "false") + ");");
//        if (updateFlags_.test(BIT_FILTERS_CHANGED) || all)
//            doJavaScript(jsRef() + ".setFilters("
//                         + jsStringLiteral(acceptAttributes_) + ");");
//        if (updateFlags_.test(BIT_DRAGOPTIONS_CHANGED) || all) {
//            doJavaScript(jsRef() + ".setDropIndication("
//                         + (dropIndicationEnabled_ ? "true" : "false") + ");");
//            doJavaScript(jsRef() + ".setDropForward("
//                         + (globalDropEnabled_ ? "true" : "false") + ");");
//        }
//        if (updateFlags_.test(BIT_JSFILTER_CHANGED) || all) {
//            createWorkerResource();

//            doJavaScript(jsRef() + ".setUploadWorker(\""
//                         + (uploadWorkerResource_ ? uploadWorkerResource_->url() : "")
//                         + "\");");
//            doJavaScript(jsRef() + ".setChunkSize("
//                         + boost::lexical_cast<std::string>(chunkSize_) + ");");
//        }

        updateFlags_.reset();
    }

    WContainerWidget::updateDom(element, all);
}

std::string WDropZone::renderRemoveJs(bool recursive) {
    if (isRendered()) {
        std::string result = jsRef() + ".destructor();";

        if (!recursive)
            result += WT_CLASS ".remove('" + id() + "');";

        return result;
    } else {
        return WContainerWidget::renderRemoveJs(recursive);
    }
}

void WDropZone::setHoverStyleClass(const std::string& className)
{
    if (className == hoverStyleClass_)
        return;

    hoverStyleClass_ = className;

    updateFlags_.set(BIT_HOVERSTYLE_CHANGED);
    repaint();
}

void WDropZone::setAcceptDrops(bool enable)
{
    if (enable == acceptDrops_)
        return;

    acceptDrops_ = enable;

    updateFlags_.set(BIT_ACCEPTDROPS_CHANGED);
    repaint();
}

void WDropZone::setFilters(const std::string& acceptAttributes)
{
    if (acceptAttributes == acceptAttributes_)
        return;

    acceptAttributes_ = acceptAttributes;

    doJavaScript(this->jsRef() + ".setAcceptedFiles(" + acceptAttributes +");");

    updateFlags_.set(BIT_FILTERS_CHANGED);
    repaint();
}

void WDropZone::setDropIndicationEnabled(bool enable) {
    if (enable == dropIndicationEnabled_)
        return;

    dropIndicationEnabled_ = enable;

    updateFlags_.set(BIT_DRAGOPTIONS_CHANGED);
    repaint();
}

bool WDropZone::dropIndicationEnabled() const {
    return dropIndicationEnabled_;
}

void WDropZone::setGlobalDropEnabled(bool enable) {
    if (enable == globalDropEnabled_)
        return;

    globalDropEnabled_ = enable;
    updateFlags_.set(BIT_DRAGOPTIONS_CHANGED);
    repaint();
}

bool WDropZone::globalDropEnabled() const {
    return globalDropEnabled_;
}

void WDropZone::setMaxSizeFile(unsigned size)
{
    doJavaScript(this->jsRef() + ".setMaxfileSize(" + std::to_string(size) + ");");
    //doJavaScript(fmt::format("{}.setMaxfileSize({});", this->jsRef(), size));
}

void WDropZone::setMaxFiles(unsigned size)
{
    doJavaScript(this->jsRef() + ".setMaxFiles(" + std::to_string(size) + ");");
}

void WDropZone::setJavaScriptFilter(const std::string& filterFn,
                                          ::uint64_t chunksize,
                                          const std::vector<std::string>& imports) {
    if (jsFilterFn_ == filterFn && chunksize == chunkSize_)
        return;

    jsFilterFn_ = filterFn;
    jsFilterImports_ = imports;
    chunkSize_ = chunksize;

    doJavaScript(this->jsRef() + ".setFilter(" + jsFilterFn_ + ");");

    updateFlags_.set(BIT_JSFILTER_CHANGED);
    repaint();
}

void WDropZone::createWorkerResource() {
    if (uploadWorkerResource_ != 0) {
        delete uploadWorkerResource_;
        uploadWorkerResource_ = 0;
    }

    if (jsFilterFn_.empty())
        return;

#ifndef WT_TARGET_JAVA
    uploadWorkerResource_ = addChild(std::make_unique<Wt::WMemoryResource>("text/javascript"));
#else // WT_TARGET_JAVA
    uploadWorkerResource_ = new WMemoryResource("text/javascript");
#endif // WT_TARGET_JAVA

    std::stringstream ss;
//    ss << "importScripts(";
//    for (unsigned i=0; i < jsFilterImports_.size(); i++) {
//        ss << "\"" << jsFilterImports_[i] << "\"";
//        if (i < jsFilterImports_.size()-1)
//            ss << ", ";
//    }
//    ss << ");" << std::endl;
//    ss << jsFilterFn_ << std::endl;

//    ss << WORKER_JS;

#ifndef WT_TARGET_JAVA
    std::string js = ss.str();
    uploadWorkerResource_->setData((const unsigned char*)js.c_str(), js.length());
#else
    try {
        uploadWorkerResource_->setData(ss.str().getBytes("utf8"));
    } catch (UnsupportedEncodingException e) {
    }
#endif
}


void WDropZone::disableJavaScriptFilter() {
    filterSupported_ = false;
}



const Http::UploadedFile &WDropZone::File::uploadedFile() const
{
    // if (!uploadFinished_)
    //     throw WException("Can not access uploaded files before upload is done.");
    // else
        return uploadedFile_;
}

void WDropZone::File::setFilterEnabled(bool enabled)
{
    filterEnabled_ = enabled;
}

WDropZone::File::File(const std::string& id, const std::string &fileName, const std::string &type, uint64_t size, uint64_t chunkSize)
      : id_(id),
        clientFileName_(fileName),
        type_(type),
        size_(size),
        uploadStarted_(false),
        uploadFinished_(false),
        cancelled_(false),
        filterEnabled_(true),
        isFiltered_(false),
        nbReceivedChunks_(0),
        chunkSize_(chunkSize)
{ }
void appendFile(const std::string &srcFile,
                const std::string &targetFile)
{
    std::ifstream ss(srcFile.c_str(), std::ios::in | std::ios::binary);
    std::ofstream ts(targetFile.c_str(), std::ios::out | std::ios::binary | std::ios::app);

    const int LEN = 4096;
    char buffer[LEN];
    while (!ss.eof()) {
        ss.read(buffer, LEN);
        ts.write(buffer, ss.gcount());
    }
}
void WDropZone::File::handleIncomingData(const Http::UploadedFile &file, bool last)
{
    if (!uploadStarted_) {
        uploadedFile_ = file;
        uploadStarted_ = true;
    } else {
        // append data to spool-file
        appendFile(file.spoolFileName(), uploadedFile_.spoolFileName());
    }
    nbReceivedChunks_++;

    if (last)
        uploadFinished_ = true;
}

void WDropZone::File::cancel()
{
    cancelled_ = true;
}

bool WDropZone::File::cancelled() const
{
    return cancelled_;
}

void WDropZone::File::emitDataReceived(uint64_t current, uint64_t total, bool filterSupported)
{
    if (!filterEnabled_ || !filterSupported || chunkSize_ == 0) {
        dataReceived_.emit(current, total);
    } else {
        ::uint64_t currentChunkSize = chunkSize_;
        unsigned nbChunks = (unsigned)(size_ / chunkSize_);
        if (nbReceivedChunks_ == nbChunks) // next chunk is the remainder
            currentChunkSize = size_ - (nbReceivedChunks_*chunkSize_);

        ::uint64_t progress = nbReceivedChunks_*chunkSize_
                              + ::uint64_t( (double(current)/double(total)) * currentChunkSize );
        dataReceived_.emit(progress, size_);
    }
}

void WDropZone::File::setIsFiltered(bool filtered)
{
    isFiltered_ = filtered;
}

} //namespace Wt
