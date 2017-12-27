//
//  main.cpp
//  String Process
//
//  Created by Henry on 28/09/2017.
//  Copyright © 2017 henry. All rights reserved.
//

#include <iostream>
#include <fstream>
#include <string.h>
#include <stdio.h>

using namespace std;

int main(int argc, const char * argv[]) {
    
    if(argc != 3)
    {
        cout << "Wrong Arguments";
        return 1;
    }
    
    char text_line[3000];    //text = search words
    fstream in_file;      //fstream 讀入pattern file
    printf("------------Input file %s------------\n", argv[1]);
    //in_file.open(argv[1], ios::in);
    string delimite = " ";
    
    
    
    printf("----------End of input file %s----------\n", argv[1]);
    printf("**************User input**************\n");
    
    cin.getline(text_line, sizeof(text_line));
    
    while( strcmp(text_line, "exit" ) )
    {
        
        char *pch = strtok(text_line, " ");
        
        if ( strcmp(pch, "reverse") == 0 )       // do reverse
        {
            pch = strtok(NULL, "\n");
            long sizeof_pch = 0;
            
            for(int i = 0; pch[i] != '\0'; i++) //計算pch的長度
            {
                sizeof_pch = i+1;
            }
            for(long i = sizeof_pch; i >= 0; i--)
            {
                printf("%c",pch[i]);
            }
            printf("\n");
        }
        else if ( strcmp(pch, "split") == 0 )    // do split
        {
            pch = strtok(NULL, "\n");
            char new_pch[3000] = "";
            int size_of_delimite = delimite.size();
            for(int i = 0; pch[i] != '\0'; i++)
            {
                bool match = true;
                int j = 0;
                for(j = 0; j != size_of_delimite; j++)
                {
                    if( pch[i+j] != delimite[j] )
                    {
                        match = false;
                        break;
                    }
                    else
                    {
                        new_pch[i+j] = ' ';
                    }
                }
                if(match == false)
                {
                    new_pch[i] = pch[i];
                }
                else
                {
                    i = i + j - 1;
                }
            }
            printf("%s\n", new_pch);
        }
        else
        {
            pch = strtok(NULL, " ");
            printf("%s\n", pch);
            pch = strtok(NULL, " ");
            printf("%d", pch == NULL);
            printf("%s\n", pch);
        }
        
        cin.getline(text_line, sizeof(text_line));

        
    }

    
    return 0;
}
