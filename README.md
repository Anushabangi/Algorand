# Algorand

Algorand protocol on ExpoDB

Group name\: Algorand

Group member\: Robert He, Hanqi Du, Chen Zhang, Zhiyi Xu

## File
- Doc \: meeting notes, archives and drafts
- Algorand \: source code and scripts for ExpoDB and Algorand
    - blockchain
        - Algorand.h, Algorand.cpp \\\\ Hanqi\: the class of Algorand
        - vrf_rsa_util.h, vrf_rsa_util.cpp \\\\ Chen\: VRF functions
    - system
        - global.h, global.cpp \\\\ Hanqi\: global resources
        - main.cpp \\\\ Robert\: initialize
        - txn.h, txn.cpp \\\\ Robert\: sortition logic in prepare message
        - worker_thread.h, worker_thread.cpp \\\\ Robert\: BA part of Algorand, message process 
    - transport
        - message.h, message.cpp \\\\ Zhiyi\: change message context for Algorand 
    - config.h \\\\ Zhiyi\: global settings
    - runL.sh dev_run.sh \\\\ Zhiyi\: shell scripts for Algorand tests
- VRF \: source code for VRF test \\\\ Chen
    - test.cpp
    - vrf_rsa_util.cpp
    - vrf_rsa_util.h

## Reference:

> Gilad, Yossi, et al. "Algorand: Scaling byzantine agreements for cryptocurrencies." Proceedings of the 26th Symposium on Operating Systems Principles. ACM, 2017.

