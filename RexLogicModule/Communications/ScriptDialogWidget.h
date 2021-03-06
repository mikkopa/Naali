// For conditions of distribution and use, see copyright notice in license.txt

/**
 *  @file   ScriptDialogWidget.cpp
 *  @brief  Dialog Widget started by llDialog script command at server.
 */

#ifndef incl_RexLogic_ScriptDialogWidget_h
#define incl_RexLogic_ScriptDialogWidget_h

#include <QWidget>
#include <QPushButton>

#include "ScriptDialogRequest.h"

namespace Foundation
{
    class Framework;
}

namespace RexLogic
{
    /**
     * Button for ScriptDialog selection. It emit Clicked event with 
     *
     */
    class SelectionButton : public QPushButton
    {
        Q_OBJECT

    public:
        //! Constructor
        //! @param id Id to identify button later when click event is triggered
        SelectionButton(QWidget *parent, const char * name = 0, QString id="" );

        //! @return id given to constructor
        QString GetId();

    private slots:
        void OnClicked();

    private:
        QString id_;

    signals:
        void Clicked(QString id);
    };

    /**
     * Dialog for presenting 1 to 12 labeled buttons and ignore button.
     * Used for requesting response for ScriptDialog network event emited by
     * llDialog script command on server side.
     *
     */
    class ScriptDialogWidget : public QWidget
    {
        Q_OBJECT

    public:
        /// Constructor.
        /// @param framework Framework pointer.
        ScriptDialogWidget(ScriptDialogRequest &request, Foundation::Framework *framework, QWidget *parent = 0);

        /// Destructor.
        virtual ~ScriptDialogWidget();

    protected:
        void hideEvent(QHideEvent *hide_event);
        void InitWindow(ScriptDialogRequest &request);

    private:
        QWidget *widget_;
        ScriptDialogRequest request_;
        //UiServices::UiProxyWidget *proxyWidget_;

    private slots:
        void OnButtonPressed(QString id);
        void OnIgnorePressed();

    signals:
        void OnClosed(s32 channel, QString answer);
    };
}

#endif // incl_RexLogic_ScriptDialogWidget_h
