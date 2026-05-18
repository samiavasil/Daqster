#include "qtrest_lib.h"
#include "apibase.h"
#include "pagination.h"
#include "abstractjsonrestlistmodel.h"
#include "jsonrestlistmodel.h"

//#include "jsonrestlistmodel.h"
#include "api/Exchange/model/ExchangeModel.h"

Qtrest_lib::Qtrest_lib()
{
}

void initializeRest()
{
    ExchangeApi::declareQML();
    ExchangeModel::declareQML();
    JsonRestListModel::declareQML();
}
