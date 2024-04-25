#!/usr/bin/octave -qf 

clear;

oldpwd=pwd;cd /home/ubuntu/Cerotti/OctaveScript/FullBNT-1.0.4;
%oldpwd=pwd;cd /home/davide/Proj/RSEWork/FullBNT-1.0.4;
addpath(genpath(pwd));
cd(oldpwd);
warning off;

% load functions to read csv
pkg load io

args=argv();
%args={"in.txt", "ris.txt", "ris.png", "ris.csv"};

if(length(args)!=4)
  printf("Usage program_name <input_file> <output_file> <image_file> <csv_file>\n");
  exit(1);
end

%Hidden variables
h_states = {'SpoofRepInfo', 'BlockRep', 'ModParRep', 'SpoofRepBF', 'ManView', 'BlockCmd', 'ModParCmd', 'ManControl', 'Compromise', 'ProgDownload', 'MITM', 'Sniffing', 'ModAuth'};
%Observable variables
obs = {'CheckIntBrokerConf', 'CheckFWInt', 'CheckSysInt', 'AllTopicSubs', 'MsgFreqBlock','MsgFreqInfo','MsgFreqBF', 'NonExTopic'};
%Array containing each node name 
names=[h_states, obs];

%Number of nodes 
n=length(names);

%Intraslice edges
intrac = {'SpoofRepInfo', 'ManView';
'SpoofRepInfo', 'MsgFreqInfo';
'BlockRep', 'ManView';
'BlockRep', 'MsgFreqBlock';
'ModParRep', 'ManView';
'SpoofRepBF', 'ManView';
'SpoofRepBF', 'MsgFreqBF';
'SpoofRepBF', 'NonExTopic';
'ManView', 'Compromise';
'BlockCmd', 'ManControl';
'ModParCmd', 'ManControl';
'ManControl', 'Compromise';
'ProgDownload', 'CheckSysInt';
'MITM', 'CheckFWInt';
'Sniffing', 'AllTopicSubs';
'ModAuth', 'CheckIntBrokerConf'};

%Making intraslice adjiacent matrix
[intra, names] = mk_adj_mat(intrac, names, 1);

%Interslice edges
interc = {'SpoofRepInfo', 'SpoofRepInfo';
'Sniffing', 'SpoofRepInfo';
'BlockRep', 'BlockRep';
'ProgDownload', 'BlockRep';
'ModParRep', 'ModParRep';
'ProgDownload', 'ModParRep';
'SpoofRepBF', 'SpoofRepBF';
'ModAuth', 'SpoofRepBF';
'BlockCmd', 'BlockCmd';
'ProgDownload', 'BlockCmd';
'ModParCmd', 'ModParCmd';
'ProgDownload', 'ModParCmd';
'ProgDownload', 'ProgDownload';
'MITM', 'ProgDownload';
'MITM', 'MITM';
'ModAuth', 'MITM';
'Sniffing', 'Sniffing';
'ModAuth', 'Sniffing';
'ModAuth', 'ModAuth'
};

%Making interslice adjiacent matrix
inter = mk_adj_mat(interc, names, 0);

% Number of states (ns(i)=x means variable i has x states)
ns = [2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2 2];

% Creating the DBN
bnet = mk_dbn(intra, inter, ns, 'names', names);

%Define variables to have value of the node name


ModAuth=bnet.names('ModAuth');
TMeanTech(ModAuth)=1.0;
PrTech(ModAuth)=0.1;

MITM=bnet.names('MITM');
TMeanTech(MITM)=2.0;
PrTech(MITM)=0.1;

Sniffing=bnet.names('Sniffing');
TMeanTech(Sniffing)=10.0;
PrTech(Sniffing)=0.1;

SpoofRepBF=bnet.names('SpoofRepBF');
TMeanTech(SpoofRepBF)=50.0;
PrTech(SpoofRepBF)=0.1;

ProgDownload=bnet.names('ProgDownload');
TMeanTech(ProgDownload)=2.0;
PrTech(ProgDownload)=0.1;

SpoofRepInfo=bnet.names('SpoofRepInfo');
TMeanTech(SpoofRepInfo)=20.0;
PrTech(SpoofRepInfo)=0.1;

BlockRep=bnet.names('BlockRep');
TMeanTech(BlockRep)=40.0;
PrTech(BlockRep)=0.1;

BlockCmd=bnet.names('BlockCmd');
TMeanTech(BlockCmd)=50.0;
PrTech(BlockCmd)=0.1;

ModParCmd=bnet.names('ModParCmd');
TMeanTech(ModParCmd)=30.0;
PrTech(ModParCmd)=0.1;

ModParRep=bnet.names('ModParRep');
TMeanTech(ModParRep)=10.0;
PrTech(ModParRep)=0.1;

ManView=bnet.names('ManView');
TMeanTech(ManView)=0.0;
PrTech(ManView)=0.1;

CheckIntBrokerConf=bnet.names('CheckIntBrokerConf');
CheckFWInt=bnet.names('CheckFWInt');
CheckSysInt=bnet.names('CheckSysInt');
AllTopicSubs=bnet.names('AllTopicSubs');
ManControl=bnet.names('ManControl');
ManView=bnet.names('ManView');
Compromise=bnet.names('Compromise');


MsgFreqBlock=bnet.names('MsgFreqBlock');
TMeanTech(MsgFreqBlock)=0.0;
PrTech(MsgFreqBlock)=0.1;

MsgFreqInfo=bnet.names('MsgFreqInfo');
TMeanTech(MsgFreqInfo)=0.0;
PrTech(MsgFreqInfo)=0.1;

MsgFreqBF=bnet.names('MsgFreqBF');
TMeanTech(MsgFreqBF)=0.0;
PrTech(MsgFreqBF)=0.1;

NonExTopic=bnet.names('NonExTopic');
TMeanTech(NonExTopic)=0.0;
PrTech(NonExTopic)=0.1;

DeltaT=1/(sum(1./TMeanTech(TMeanTech>0)) )
PrAttDef = DeltaT./TMeanTech;


TCont=100 % at least 400 to achieve accurate results in Mean Time 
T = ceil(TCont/DeltaT)


% Creating the CPDs

%%%%%%%%% ------- slice 1 -------

%node SpoofRepInfo(id=SpoofRepInfo) slice 1 
%parent order:{}
cpt(:,:)=[1.0, 0.0];
bnet.CPD{bnet.names('SpoofRepInfo')}=tabular_CPD(bnet,bnet.names('SpoofRepInfo'),'CPT',cpt);
clear cpt;

%node BlockRep(id=BlockRep) slice 1 
%parent order:{}
cpt(:,:)=[1.0, 0.0];
bnet.CPD{bnet.names('BlockRep')}=tabular_CPD(bnet,bnet.names('BlockRep'),'CPT',cpt);
clear cpt;

%node ModParRep(id=ModParRep) slice 1 
%parent order:{}
cpt(:,:)=[1.0, 0.0];
bnet.CPD{bnet.names('ModParRep')}=tabular_CPD(bnet,bnet.names('ModParRep'),'CPT',cpt);
clear cpt;

%node SpoofRepBF(id=SpoofRepBF) slice 1 
%parent order:{}
cpt(:,:)=[1.0, 0.0];
bnet.CPD{bnet.names('SpoofRepBF')}=tabular_CPD(bnet,bnet.names('SpoofRepBF'),'CPT',cpt);
clear cpt;

%node ManView(Or)(id=ManView) slice 1 
%parent order:{SpoofRepInfo, BlockRep, ModParRep, SpoofRepBF}
cpt(1,1,1,1,:)=[1.0, 0.0];
cpt(1,1,1,2,:)=[0.0, 1.0];
cpt(1,1,2,1,:)=[0.0, 1.0];
cpt(1,1,2,2,:)=[0.0, 1.0];
cpt(1,2,1,1,:)=[0.0, 1.0];
cpt(1,2,1,2,:)=[0.0, 1.0];
cpt(1,2,2,1,:)=[0.0, 1.0];
cpt(1,2,2,2,:)=[0.0, 1.0];
cpt(2,1,1,1,:)=[0.0, 1.0];
cpt(2,1,1,2,:)=[0.0, 1.0];
cpt(2,1,2,1,:)=[0.0, 1.0];
cpt(2,1,2,2,:)=[0.0, 1.0];
cpt(2,2,1,1,:)=[0.0, 1.0];
cpt(2,2,1,2,:)=[0.0, 1.0];
cpt(2,2,2,1,:)=[0.0, 1.0];
cpt(2,2,2,2,:)=[0.0, 1.0];
cpt1=mk_named_CPT({'SpoofRepInfo', 'BlockRep', 'ModParRep', 'SpoofRepBF', 'ManView'},names, bnet.dag, cpt);
bnet.CPD{bnet.names('ManView')}=tabular_CPD(bnet,bnet.names('ManView'),'CPT',cpt1);
clear cpt;clear cpt1;

%node BlockCmd(id=BlockCmd) slice 1 
%parent order:{}
cpt(:,:)=[1.0, 0.0];
bnet.CPD{bnet.names('BlockCmd')}=tabular_CPD(bnet,bnet.names('BlockCmd'),'CPT',cpt);
clear cpt;

%node ModParCmd(id=ModParCmd) slice 1 
%parent order:{}
cpt(:,:)=[1.0, 0.0];
bnet.CPD{bnet.names('ModParCmd')}=tabular_CPD(bnet,bnet.names('ModParCmd'),'CPT',cpt);
clear cpt;

%node ManControl(Or)(id=ManControl) slice 1 
%parent order:{BlockCmd, ModParCmd}
cpt(1,1,:)=[1.0, 0.0];
cpt(1,2,:)=[0.0, 1.0];
cpt(2,1,:)=[0.0, 1.0];
cpt(2,2,:)=[0.0, 1.0];
cpt1=mk_named_CPT({'BlockCmd', 'ModParCmd', 'ManControl'},names, bnet.dag, cpt);
bnet.CPD{bnet.names('ManControl')}=tabular_CPD(bnet,bnet.names('ManControl'),'CPT',cpt1);
clear cpt;clear cpt1;

%node Compromise(Or)(id=Compromise) slice 1 
%parent order:{ManView, ManControl}
cpt(1,1,:)=[1.0, 0.0];
cpt(1,2,:)=[0.0, 1.0];
cpt(2,1,:)=[0.0, 1.0];
cpt(2,2,:)=[0.0, 1.0];
cpt1=mk_named_CPT({'ManView', 'ManControl', 'Compromise'},names, bnet.dag, cpt);
bnet.CPD{bnet.names('Compromise')}=tabular_CPD(bnet,bnet.names('Compromise'),'CPT',cpt1);
clear cpt;clear cpt1;

%node ProgDownload(id=ProgDownload) slice 1 
%parent order:{}
cpt(:,:)=[1.0, 0.0];
bnet.CPD{bnet.names('ProgDownload')}=tabular_CPD(bnet,bnet.names('ProgDownload'),'CPT',cpt);
clear cpt;

%node MITM(id=MITM) slice 1 
%parent order:{}
cpt(:,:)=[1.0, 0.0];
bnet.CPD{bnet.names('MITM')}=tabular_CPD(bnet,bnet.names('MITM'),'CPT',cpt);
clear cpt;

%node Sniffing(id=Sniffing) slice 1 
%parent order:{}
cpt(:,:)=[1.0, 0.0];
bnet.CPD{bnet.names('Sniffing')}=tabular_CPD(bnet,bnet.names('Sniffing'),'CPT',cpt);
clear cpt;

%node ModAuth(id=ModAuth) slice 1 
%parent order:{}
cpt(:,:)=[1.0, 0.0];
bnet.CPD{bnet.names('ModAuth')}=tabular_CPD(bnet,bnet.names('ModAuth'),'CPT',cpt);
clear cpt;

%node CheckIntBrokerConf(id=CheckIntBrokerConf) slice 1 
%parent order:{ModAuth}
cpt(1,:)=[0.99, 0.01000000000000001];
cpt(2,:)=[0.01000000000000001, 0.99];
bnet.CPD{bnet.names('CheckIntBrokerConf')}=tabular_CPD(bnet,bnet.names('CheckIntBrokerConf'),'CPT',cpt);
clear cpt;

%node CheckFWInt(id=CheckFWInt) slice 1 
%parent order:{MITM}
cpt(1,:)=[0.99, 0.01000000000000001];
cpt(2,:)=[0.01000000000000001, 0.99];
bnet.CPD{bnet.names('CheckFWInt')}=tabular_CPD(bnet,bnet.names('CheckFWInt'),'CPT',cpt);
clear cpt;

%node AllTopicSubs(id=AllTopicSubs) slice 1 
%parent order:{Sniffing}
cpt(1,:)=[0.99, 0.01000000000000001];
cpt(2,:)=[0.01000000000000001, 0.99];
bnet.CPD{bnet.names('AllTopicSubs')}=tabular_CPD(bnet,bnet.names('AllTopicSubs'),'CPT',cpt);
clear cpt;

%node CheckSysInt(id=CheckSysInt) slice 1 
%parent order:{ProgDownload}
cpt(1,:)=[0.99, 0.01000000000000001];
cpt(2,:)=[0.01000000000000001, 0.99];
bnet.CPD{bnet.names('CheckSysInt')}=tabular_CPD(bnet,bnet.names('CheckSysInt'),'CPT',cpt);
clear cpt;

%node MsgFreqBlock(id=MsgFreqBlock) slice 1 
%parent order:{BlockRep}
cpt(1,:)=[0.99, 0.01000000000000001];
cpt(2,:)=[0.01000000000000001, 0.99];
bnet.CPD{bnet.names('MsgFreqBlock')}=tabular_CPD(bnet,bnet.names('MsgFreqBlock'),'CPT',cpt);
clear cpt;

%node MsgFreqInfo(id=MsgFreqInfo) slice 1 
%parent order:{SpoofRepInfo}
cpt(1,:)=[0.99, 0.01000000000000001];
cpt(2,:)=[0.01000000000000001, 0.99];
bnet.CPD{bnet.names('MsgFreqInfo')}=tabular_CPD(bnet,bnet.names('MsgFreqInfo'),'CPT',cpt);
clear cpt;

%node MsgFreqBF(id=MsgFreqBF) slice 1 
%parent order:{SpoofRepBF}
cpt(1,:)=[0.99, 0.01000000000000001];
cpt(2,:)=[0.01000000000000001, 0.99];
bnet.CPD{bnet.names('MsgFreqBF')}=tabular_CPD(bnet,bnet.names('MsgFreqBF'),'CPT',cpt);
clear cpt;

%node NonExTopic(id=NonExTopic) slice 1 
%parent order:{SpoofRepBF}
cpt(1,:)=[0.99, 0.01000000000000001];
cpt(2,:)=[0.01000000000000001, 0.99];
bnet.CPD{bnet.names('NonExTopic')}=tabular_CPD(bnet,bnet.names('NonExTopic'),'CPT',cpt);
clear cpt;


%%%%%%%%% ------- slice 2 --------

%node SpoofRepInfo(id=SpoofRepInfo) slice 2 
%parent order:{SpoofRepInfo, Sniffing}
CurPrAtt = PrAttDef(SpoofRepInfo)
cpt(1,1,:)=[1.0, 0.0];
cpt(1,2,:)=[1-CurPrAtt, CurPrAtt];
%cpt(1,2,:)=[0.884057971014493, 0.115942028985507];
cpt(2,1,:)=[0.0, 1.0];
cpt(2,2,:)=[0.0, 1.0];
cpt1=mk_named_CPT_inter({'SpoofRepInfo', 'Sniffing', 'SpoofRepInfo'},names, bnet.dag, cpt,[]);
bnet.CPD{bnet.eclass2(bnet.names('SpoofRepInfo'))}=tabular_CPD(bnet,n+bnet.names('SpoofRepInfo'),'CPT',cpt1);
clear cpt; clear cpt1;

%node BlockRep(id=BlockRep) slice 2 
%parent order:{BlockRep, ProgDownload}
CurPrAtt = PrAttDef(BlockRep);
cpt(1,1,:)=[1.0, 0.0];
cpt(1,2,:)=[1-CurPrAtt, CurPrAtt];
%cpt(1,2,:)=[0.927536231884058, 0.072463768115942];
cpt(2,1,:)=[0.0, 1.0];
cpt(2,2,:)=[0.0, 1.0];
cpt1=mk_named_CPT_inter({'BlockRep', 'ProgDownload', 'BlockRep'},names, bnet.dag, cpt,[]);
bnet.CPD{bnet.eclass2(bnet.names('BlockRep'))}=tabular_CPD(bnet,n+bnet.names('BlockRep'),'CPT',cpt1);
clear cpt; clear cpt1;

%node ModParRep(id=ModParRep) slice 2 
%parent order:{ModParRep, ProgDownload}
CurPrAtt = PrAttDef(ModParRep);
cpt(1,1,:)=[1.0, 0.0];
cpt(1,2,:)=[1-CurPrAtt, CurPrAtt];
%cpt(1,2,:)=[0.927536231884058, 0.072463768115942];
cpt(2,1,:)=[0.0, 1.0];
cpt(2,2,:)=[0.0, 1.0];
cpt1=mk_named_CPT_inter({'ModParRep', 'ProgDownload', 'ModParRep'},names, bnet.dag, cpt,[]);
bnet.CPD{bnet.eclass2(bnet.names('ModParRep'))}=tabular_CPD(bnet,n+bnet.names('ModParRep'),'CPT',cpt1);
clear cpt; clear cpt1;

%node SpoofRepBF(id=SpoofRepBF) slice 2 
%parent order:{SpoofRepBF, ModAuth}
CurPrAtt = PrAttDef(SpoofRepBF);
cpt(1,1,:)=[1.0, 0.0];
cpt(1,2,:)=[1-CurPrAtt, CurPrAtt];
%cpt(1,2,:)=[0.884057971014493, 0.115942028985507];
cpt(2,1,:)=[0.0, 1.0];
cpt(2,2,:)=[0.0, 1.0];
cpt1=mk_named_CPT_inter({'SpoofRepBF', 'ModAuth', 'SpoofRepBF'},names, bnet.dag, cpt,[]);
bnet.CPD{bnet.eclass2(bnet.names('SpoofRepBF'))}=tabular_CPD(bnet,n+bnet.names('SpoofRepBF'),'CPT',cpt1);
clear cpt; clear cpt1;

%node BlockCmd(id=BlockCmd) slice 2 
%parent order:{BlockCmd, ProgDownload}
CurPrAtt = PrAttDef(BlockCmd);
cpt(1,1,:)=[1.0, 0.0];
cpt(1,2,:)=[1-CurPrAtt, CurPrAtt];
%cpt(1,2,:)=[0.927536231884058, 0.072463768115942];
cpt(2,1,:)=[0.0, 1.0];
cpt(2,2,:)=[0.0, 1.0];
cpt1=mk_named_CPT_inter({'BlockCmd', 'ProgDownload', 'BlockCmd'},names, bnet.dag, cpt,[]);
bnet.CPD{bnet.eclass2(bnet.names('BlockCmd'))}=tabular_CPD(bnet,n+bnet.names('BlockCmd'),'CPT',cpt1);
clear cpt; clear cpt1;

%node ModParCmd(id=ModParCmd) slice 2 
%parent order:{ModParCmd, ProgDownload}
CurPrAtt = PrAttDef(ModParCmd);
cpt(1,1,:)=[1.0, 0.0];
cpt(1,2,:)=[1-CurPrAtt, CurPrAtt];
%cpt(1,2,:)=[0.927536231884058, 0.072463768115942];
cpt(2,1,:)=[0.0, 1.0];
cpt(2,2,:)=[0.0, 1.0];
cpt1=mk_named_CPT_inter({'ModParCmd', 'ProgDownload', 'ModParCmd'},names, bnet.dag, cpt,[]);
bnet.CPD{bnet.eclass2(bnet.names('ModParCmd'))}=tabular_CPD(bnet,n+bnet.names('ModParCmd'),'CPT',cpt1);
clear cpt; clear cpt1;

%node ProgDownload(id=ProgDownload) slice 2 
%parent order:{ProgDownload, MITM}
CurPrAtt = PrAttDef(ProgDownload);
cpt(1,1,:)=[1.0, 0.0];
%cpt(1,2,:)=[0.884057971014493, 0.115942028985507];
cpt(1,2,:)=[1-CurPrAtt, CurPrAtt];
%cpt(1,2,:)=[1-CurPrAtt/2, CurPrAtt/2];
%cpt(1,2,:)=[1-CurPrAtt/3, CurPrAtt/3];
%cpt(1,2,:)=[1-CurPrAtt/4, CurPrAtt/4];
%cpt(1,2,:)=[1-CurPrAtt/5, CurPrAtt/5];
cpt(2,1,:)=[0.0, 1.0];
cpt(2,2,:)=[0.0, 1.0];
cpt1=mk_named_CPT_inter({'ProgDownload', 'MITM', 'ProgDownload'},names, bnet.dag, cpt,[]);
bnet.CPD{bnet.eclass2(bnet.names('ProgDownload'))}=tabular_CPD(bnet,n+bnet.names('ProgDownload'),'CPT',cpt1);
clear cpt; clear cpt1;

%node MITM(id=MITM) slice 2 
%parent order:{MITM, ModAuth}
CurPrAtt = PrAttDef(MITM);
cpt(1,1,:)=[1.0, 0.0];
cpt(1,2,:)=[1-CurPrAtt, CurPrAtt];
%cpt(1,2,:)=[0.898550724637681, 0.101449275362319];
cpt(2,1,:)=[0.0, 1.0];
cpt(2,2,:)=[0.0, 1.0];
cpt1=mk_named_CPT_inter({'MITM', 'ModAuth', 'MITM'},names, bnet.dag, cpt,[]);
bnet.CPD{bnet.eclass2(bnet.names('MITM'))}=tabular_CPD(bnet,n+bnet.names('MITM'),'CPT',cpt1);
clear cpt; clear cpt1;

%node Sniffing(id=Sniffing) slice 2 
%parent order:{Sniffing, ModAuth}
CurPrAtt = PrAttDef(Sniffing);
cpt(1,1,:)=[1.0, 0.0];
cpt(1,2,:)=[1-CurPrAtt, CurPrAtt];
%cpt(1,2,:)=[0.855072463768116, 0.144927536231884];
cpt(2,1,:)=[0.0, 1.0];
cpt(2,2,:)=[0.0, 1.0];
cpt1=mk_named_CPT_inter({'Sniffing', 'ModAuth', 'Sniffing'},names, bnet.dag, cpt,[]);
bnet.CPD{bnet.eclass2(bnet.names('Sniffing'))}=tabular_CPD(bnet,n+bnet.names('Sniffing'),'CPT',cpt1);
clear cpt; clear cpt1;

%node ModAuth(id=ModAuth) slice 2 
%parent order:{ModAuth}
CurPrAtt = PrAttDef(ModAuth);
cpt(1,:)=[1-CurPrAtt, CurPrAtt];
%cpt(1,:)=[0.884057971014493, 0.115942028985507];
cpt(2,:)=[0.0, 1.0];
bnet.CPD{bnet.eclass2(bnet.names('ModAuth'))}=tabular_CPD(bnet,n+bnet.names('ModAuth'),'CPT',cpt);
clear cpt; 

% choose the inference engine
ec='JT';

% ff=0 --> no fully factorized  OR ff=1 --> fully factorized
ff=0;

% list of clusters
if (ec=='JT')
    engine=jtree_dbn_inf_engine(bnet); %exact inference with JT alg.
% 	engine=bk_inf_engine(bnet, 'clusters', 'exact') %exact inference with
%	bk alg. is equivalent to JT, but more RAM used until saturation 
else
	if (ff==1)
		engine=bk_inf_engine(bnet, 'clusters', 'ff'); % fully factorized
	else
		clusters={[]};
		engine=bk_inf_engine(bnet, 'clusters', clusters);
	end
end



% IMPORTANT: GeNIe start slices from 0,
tStep=1; %Time Step
evidence=cell(n,T); % create the evidence cell array


% Evidence
% 
% FIRST EXPERIMENT:
% NO EVIDENCE
% FIG: NoEvidences.jpg
% COMMENTS: 

% SECOND EXPERIMENT:
% ALL ANALYTICS OFF FOR ALL TIME
% FIG: 
% COMMENTS: now the observation drives the model: given a negligible
% (10^-4) pr of false negative of the analytics, with all of them off we
% are sure that no attacks are in course


c = csv2cell(args{1});

if(!isempty(c))
  for i=1:length(c(:,1))
    for t=0:T 
    %  A PARTIRE DALLA LINEA SUCCESSIVA
        evidence{bnet.names(c{i,1}),t+1}=c{i,2};
    %    evidence{CheckIntBrokerConf,t+1}=1;
    %    evidence{CheckFWInt,t+1}=2;
    %    evidence{CheckSysInt,t+1}=1;
    %    evidence{AllTopicSubs,t+1}=1;
    %  FIN QUI
    end
  end
end


% THIRD EXPERIMENT:
% CheckIntBrokerConf ACTIVATE @ T=30, NO OTHERS OBSERVATIONS
% FIG: 
% COMMENTS: without evidences, until t=30 the situation is uncert... ,
% the MITM is the most likely threat source, but has a low chance to lead to a succesful ICS compromission.
% Then, the activation of Periodic suggests that the attacker/intruder is using a SpoofRepMes thus incresing both the pr of ICS compr and CorrReact 
% for t=0:ceil(30/DeltaT)-1
%     evidence{CheckIntBrokerConf,t+1}=1;
% end
% for t=ceil(30/DeltaT):T
%     evidence{CheckIntBrokerConf,t+1}=2;
% end

% FOURTH EXPERIMENT:
% CheckIntBrokerConf always active
% FIG: 
% % COMMENTS: 
% for t=1:T
%     evidence{CheckIntBrokerConf,t+1}=2;
% end

fid = stdout; 
% fopen(args{2},'w');

% DC: collect results to plot in matrix Res
Res = zeros(n, T);
t1=cputime;
% Evidence
% first cells of evidence are for time 0
% Inference algorithm (filtering / smoothing)
filtering=0;
% filtering=0 --> smoothing (is the default - enter_evidence(engine,evidence))
% filtering=1 --> filtering
if ~filtering
	fprintf(fid, '\n*****  SMOOTHING *****\n\n');
else
	fprintf(fid, '\n*****  FILTERING *****\n\n');
end

if(ec=='JT')
    [engine, loglik] = enter_evidence(engine, evidence);
else
    [engine, loglik] = enter_evidence(engine, evidence, 'filter', filtering);
end

% analysis time is t for anterior nodes and t+1 for ulterior nodes
for t=1:tStep:T-1
%t = analysis time

% create the vector of marginals
% marg(i).T is the posterior distribution of node T
% with marg(i).T(false) and marg(i).T(true)

% NB. if filtering then ulterior nodes cannot be marginalized at time t=1

if ~filtering
	for i=1:(n*2)
		marg(i)=marginal_nodes(engine, i , t);
	end
else
	if t==1
		for i=1:n
			marg(i)=marginal_nodes(engine, i, t);
		end
	else
		for i=1:(n*2)
			marg(i)=marginal_nodes(engine, i, t);
		end
	end
end

% Printing results
% IMPORTANT: To be consistent with GeNIe we start counting/printing time slices from 0


% Anterior nodes are printed from t=1 to T-1
fprintf(fid, '\n\n**** Time %i *****\n****\n\n',t-1);
%fprintf(fid, '*** Anterior nodes \n');
for i=1:n
	if isempty(evidence{i,t})
		for k=1:ns(i)
			fprintf(fid, 'Posterior of node %i:%s value %i : %d\n',i, names{i}, k, marg(i).T(k));
		end
			% DC collect res
			Res(i,t) = marg(i).T(2);
			fprintf(fid, '**\n');
		else
			fprintf(fid, 'Node %i:%s observed at value: %i\n**\n',i,names{i}, evidence{i,t});
		end
	end
end

% DC: Plot results
X=[0:tStep:T-2];
fg=figure;

node = {Compromise, ManView, ManControl, ModAuth, SpoofRepBF, Sniffing, ProgDownload};
% node = {ModAuth, SpoofRepBF, Sniffing, MITM, ProgDownload};
% node = {ModAuth, Compromise, ManControl, ManView}

nodestr=cell(1,length(node));
for i = [1:length(node)]
    nodestr(i) = names(node{i});
end

markers = {'o';'+';'*';'s';'d';'v';'>';'h'};
colours = {'b';'g';'r';'k';'c';'m';'g';'b'};
linestyle = {'-';'--';'-.';':';'-';'--';'-.';':'};

for i = [1:length(node)]
    hold on;
%     set(gca,'LineStyle',markers(n))
    plot(DeltaT*X, Res(node{i},1:tStep:end-1),strcat(strcat(linestyle{i},markers{i}),colours{i}));
    fprintf('TMean %s: %f \n', names{node{i}}, sum(1.-Res(node{i},1:tStep:end-1))*DeltaT);
end;
legend(nodestr,'Location','southeast');

hold off;

% title('CheckIntBrokerConf activated @ T=30')
xlabel('t')
ylabel('Pr')
ylim([0, 1.2])
set(gca,'XTick', [0:10:TCont])
set(gca,'YTick', [0:0.1:1.0])
grid on
saveas(fg, args{3})
warning off;

% Save results in csv file

fid2 = fopen(args{4},'w');
fprintf(fid2, '%s\n', csvconcat(["t",names]) );
fclose(fid2);
dlmwrite (args{4}, [DeltaT*X;Res(:,1:tStep:end-1)]', "-append");
