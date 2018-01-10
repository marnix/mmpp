#pragma once

#include <vector>
#include <memory>
#include <map>
#include <cstdint>
#include <random>

#include "utils/utils.h"
#include "parsing/parser.h"
#include "parsing/unif.h"
#include "mm/library.h"
#include "mm/toolbox.h"

class SentenceNode;
class StepNode;
class UCTProver;

enum VisitResult {
    PROVED,
    CONTINUE,
    DEAD,
};

class UCTProver : public enable_create< UCTProver > {
public:
    VisitResult visit();
    const std::vector< ParsingTree2< SymTok, LabTok > > &get_hypotheses() const;
    const std::vector< const Assertion* > &get_useful_asses() const;
    LibraryToolbox &get_toolbox() const;
    std::ranlux48 &get_rand();

protected:
    UCTProver(LibraryToolbox &tb, const ParsingTree2< SymTok, LabTok > &thesis, const std::vector< ParsingTree2< SymTok, LabTok > > &hypotheses);
    ~UCTProver();
    void init();

private:
    void compute_useful_assertions();

    std::shared_ptr< SentenceNode > root;
    LibraryToolbox &tb;
    ParsingTree2< SymTok, LabTok > thesis;
    std::vector< ParsingTree2< SymTok, LabTok > > hypotheses;
    std::vector< const Assertion* > useful_asses;
    std::ranlux48 rand;
};

class SentenceNode : public enable_create< SentenceNode > {
public:
    VisitResult visit();
    float get_value();
    uint32_t get_visit_num();
    std::weak_ptr< StepNode > get_parent();
    const ParsingTree2< SymTok, LabTok > &get_sentence();

protected:
    SentenceNode(std::weak_ptr< UCTProver > uct, std::weak_ptr< StepNode > parent, const ParsingTree2< SymTok, LabTok > &sentence);
    ~SentenceNode();

private:
    std::weak_ptr< UCTProver > uct;
    std::vector< std::shared_ptr< StepNode > > children;
    std::weak_ptr< StepNode > parent;

    ParsingTree2< SymTok, LabTok > sentence;
    uint32_t visit_num = 0;
    bool exhausted = false;
    float value = 0.0;
    float total_children_value = 0.0;
    std::vector< const Assertion* >::const_iterator ass_it;
};

class StepNode : public enable_create< StepNode > {
public:
    VisitResult visit();
    float get_value();
    uint32_t get_visit_num();
    std::weak_ptr< SentenceNode > get_parent();

protected:
    StepNode(std::weak_ptr< UCTProver > uct, std::weak_ptr< SentenceNode > parent, LabTok label, const SubstMap2< SymTok, LabTok > &const_subst_map);
    ~StepNode();

private:
    VisitResult create_child(const ParsingTree2< SymTok, LabTok > &sent);
    VisitResult create_children();
    VisitResult visit_child(size_t i);

    std::weak_ptr< UCTProver > uct;
    std::vector< std::shared_ptr< SentenceNode > > children;
    std::weak_ptr< SentenceNode > parent;
    std::vector< std::shared_ptr< SentenceNode > > active_children;
    size_t worst_child = 0;

    LabTok label;
    SubstMap2< SymTok, LabTok > const_subst_map;
    SubstMap2< SymTok, LabTok > unconst_subst_map;
    bool exhausted = false;
    /*float value = 0.0;
    uint32_t visit_num = 0;*/
};
