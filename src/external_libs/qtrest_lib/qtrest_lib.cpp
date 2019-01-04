#include "qtrest_lib.h"
#include"qtrest/src/apibase.h"
#include"qtrest/src/models/pagination.h"
#include"qtrest/src/models/abstractjsonrestlistmodel.h"
#include"qtrest/src/models/jsonrestlistmodel.h"

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
