#pragma once

#include <vector>

namespace ttk {

    namespace ParallelMergeSort {

        template<typename IdType, typename Rank1Type, typename Rank2Type>
        int merge(
            IdType* indices,
            const IdType l,
            const IdType m,
            const IdType r,
            const Rank1Type* rank1,
            const Rank2Type* rank2
        ){
            // std::cout<<"merge "<<l<<" - "<<m<<" - "<<r <<std::endl;

            // for(IdType i=l; i<=r; i++)
            //     std::cout<<indices[i]<<" ";
            // std::cout<<"\n";

            // return 1;

            IdType n1 = m - l + 1;
            IdType n2 = r - m;

            std::vector<IdType> L(n1);
            std::vector<IdType> R(n2);

            for(IdType i = 0; i<n1; i++)
                L[i] = indices[l + i];

            for(IdType j = 0; j<n2; j++)
                R[j] = indices[m + 1+ j];

            /* Merge the temp arrays back into arr[l..r]*/
            IdType i = 0; // Initial index of first subarray
            IdType j = 0; // Initial index of second subarray
            IdType k = l; // Initial index of merged subarray
            while(i<n1 && j<n2){
                // std::cout<<i<<" "<<j<<std::endl;
                const IdType& v = L[i];
                const IdType& u = R[j];
                // std::cout<<"->"<<v<<" "<<u<<std::endl;
                if(
                    (rank1[v]==rank1[u] ? (rank2[v]<rank2[u]) : (rank1[v]<rank1[u]))
                ){
                    indices[k] = v;
                    i++;
                } else {
                    indices[k] = u;
                    j++;
                }
                k++;
            }

            /* Copy the remaining elements of L[], if there
              are any */
            while(i<n1){
                indices[k] = L[i];
                i++;
                k++;
            }

            /* Copy the remaining elements of R[], if there
              are any */
            while(j<n2){
                indices[k] = R[j];
                j++;
                k++;
            }

            // for(IdType i=l; i<=r; i++)
            //     std::cout<<indices[i]<<" ";
            // std::cout<<"\n";

            return 1;
        }

        template<typename IdType, typename Rank1Type, typename Rank2Type>
        int mergeSort(
            IdType* indices,
            const IdType l,
            const IdType r,
            const Rank1Type* rank1,
            const Rank2Type* rank2
        ) {
            if(l<r){
                int m = l+(r-l)/2;

                // std::cout<<"l: " <<l<<" m: " <<m<<" r: " <<r<<std::endl;
                ttk::ParallelMergeSort::mergeSort<IdType,Rank1Type,Rank2Type>(indices, l, m, rank1, rank2);

                ttk::ParallelMergeSort::mergeSort<IdType,Rank1Type,Rank2Type>(indices, m+1, r, rank1, rank2);

                ttk::ParallelMergeSort::merge<IdType,Rank1Type,Rank2Type>(indices, l, m, r, rank1, rank2);
            }
            return 1;
        }

        template<typename IdType, typename Rank1Type, typename Rank2Type>
        int sort(
            IdType* indices,
            const IdType nIndices,
            const Rank1Type* rank1,
            const Rank2Type* rank2
        ){
            ttk::ParallelMergeSort::mergeSort<IdType,Rank1Type,Rank2Type>(indices,0,nIndices-1,rank1,rank2);
            return 1;
        }
    }
}