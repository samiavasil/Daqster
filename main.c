#include <stdio.h>

int main(void)
{
    double result, num;
    char op;
    char input_type; //Очакван входен тип - 0 е число 1 е операция
    
    printf("\nКалкулатор изпълнява следните операции: ");
    printf("\nсъбиране +");
    printf("\nизваждане -");
    printf("\nумножение *");
    printf("\nделение /");
    printf("\nчистане на резултата c");
    printf("\nизход q");
    //Четем число
    printf("\n\nНачало Въведи число: "); 
    scanf("%lf", &result);
    input_type = 1;
    do
    {   
        if (input_type == 1) {
            //printf("\nВъведи операция: ");
            scanf(" %c", &op);
            
            //Проверка за въведена коректна операция
            switch (op) {
                case '+': 
                case '-': 
                case '*': 
                case '/': 
                   //Задаваме входен тип на число
                   input_type = 0;
                   break;
                //Ако въведем 'c' изчистваме текущия резултат и го задаваме на 0                    
                case 'c':{ 
                    printf("Начало Въведи число: "); 
                    scanf("%lf", &result);
                    //Задаваме входен тип на операция 
                    input_type = 1;
                    continue; // Това знаеш ли какво прави ? 
                            // спира текущата итерация на цикъла и се връща на началото на цикъла
                }
                //Ако въведем 'q' спираме програмата
                case 'q':
                    break;
                default:
                    //Ако въведем невалидна операция се връщаме на началото на цикъла
                    printf("\nНевалидна операция - въведи +-*/cq  3");
                    //Задаваме входен тип на операция (т.е. пак питаме за операция)
                    input_type = 1;       
                    continue;
            }

        } else {
            //printf("\nВъведи число: ");
            //Четем число
            scanf("%lf", &num);
            //Задаваме входен тип на операция
            input_type = 1;
            //Извършваме операцията
            switch (op) {
                case '+': 
                    result += num;
                    break;
                case '-': 
                    result -= num;
                    break;
                case '*': 
                    result *= num;
                    break;
                case '/': 
                    if (num == 0) {
                        printf("\nДеление на 0 е невъзможно - Въведи число > 0:   ");
                        input_type = 0;
                        continue;
                    }
                    result /= num;
                    break;
            }
            //Принтираме текущия резултат
            printf("\nRESULT= %lf > ", result);
        }

        
    } while ( op != 'q');

    return 0;
}
