
#ifndef __CONMAN_SCHEME_H
#define __CONMAN_SCHEME_H

#include <conman_proto/conman.h>

namespace conman
{
  //! Manager for loading/unloading and starting/stopping Blocks
  class Scheme : public RTT::TaskContext
  {
  public:
    Scheme(std::string name="Scheme");

    ///////////////////////////////////////////////////////////////////////////
    //! \name Scheme Construction
    //\{
    
    /** \brief Add a block that has already been constructed
     *
     * After calling add_block, new_block will be a peer of this Scheme.
     *
     */
    bool add_block(RTT::TaskContext *new_block);

    /** \brief Add a block which is already a peer of this component by name.
     *
     * The block with this name must already be a peer of this Scheme.
     *
     */
    bool add_block(const std::string &name);

    //! Get the names of all the blocks in this scheme
    const std::vector<std::string> & get_blocks() {
      return block_names_;
    }

    //\}

    ///////////////////////////////////////////////////////////////////////////
    /* \name Scheme Block Group Management
     *
     * TODO: Rename 'group to 'meta_block'?
     * TODO: Implement this
     *
     * Block groups can be treated like "meta blocks" in that they can be
     * switched on and off by name. A block can belong to no groups, or it can
     * belong to many groups.
     *
     * Unlike normal blocks, since meta blocks aren't represented by TaskContext
     * pointers, they can on be enabled or disabled by name.
     *
     */
    //\{

    //! Add blocks to a group 
    bool define_group(
        const std::string &group_name,
        std::vector<RTT::TaskContext*> &grouped_blocks) { return false; }
    //! Get the blocks in a given group
    bool get_group(
        const std::string &group_name,
        std::vector<RTT::TaskContext*> &grouped_blocks) { return false; }
    //! 
    bool disband_group( const std::string &group_name) { return false; }

    //\}
    
    ///////////////////////////////////////////////////////////////////////////
    /* \name Block conflict computation
     *
     * Block conflicts are inferred from RTT data port exclusivity, as declared
     * by each given block.
     *
     * TODO: Use boost::edge_range to get all edges between this block and other
     * blocks, and enable connect the ports associated with each edge
     */
    //\{
    
    //! Compute the conflicts between all blocks in the scheme
    void compute_conflicts() {};
    //! Compute the conflicts with a single block in the scheme
    void compute_conflicts(RTT::TaskContext *block) {};
    //! Compute the conflicts with a single block in the scheme by name
    void compute_conflicts(const std::string &block_name) {};
    //! Compute the conflicts with a list of blocks in the scheme by name
    void compute_conflicts(const std::vector<std::string> &block_names) {};

    //\}

    ///////////////////////////////////////////////////////////////////////////
    /* \name Runtime Scheme Control
     *
     * <b> Enabling Blocks </b>
     *
     * "Enabling" a block corresponds to starting it through RTT's normal
     * startHook() mechanism. If it is currently running, it is considered
     * enabled.
     *
     * This is where block conflicts get checked. If enabling this port
     * conflicts with another block (like if enabling it would violate the
     * exclusivity of an input port), then it will not be enabled unless force
     * is specificed as true. If force is specified, then any conflicting
     * blocks will be disabled, themselves.
     *
     * This attempts to start() the block, which requires that the block be
     * configured before enable_block() is called. This is because calls to
     * configure() may block for unacceptable lengths of time.
     *
     *
     * <b> Disabling Blocks </b>
     *
     * "Disabling" a block corresponds to stopping it through RTT's normal
     * stopHook() mechanism. If it is not currently running, a block is
     * considered disabled. 
     *
     */
    //\{

    //! Enable a single conman Block
    bool enable_block(RTT::TaskContext *block, const bool force);
    //! Enable a single Conman block by name
    bool enable_block(const std::string &block_name, const bool force);
    //! Enable multiple Conman blocks by name simultaneously
    bool enable_blocks(
        const std::vector<std::string> &block_names, 
        const bool strict,
        const bool force);

    //! Disable a single conman Block
    bool disable_block(RTT::TaskContext *block);
    //! Disable a single Conman block by name
    bool disable_block(const std::string &block_name);
    //! Disable multiple Conman blocks by name simultaneously
    bool disable_blocks(
        const std::vector<std::string> &block_names,
        const bool strict);

    /** \brief Try to disable a set of blocks and enable another set of blocks
     *
     * NOTE: This function first disables the blocks on the disable list, and
     * then it enables blocks on the enable list.
     * 
     * \param strict Break on error if true, otherwise, try to switch the modes
     * of all blocks listed even if some fail.
     *
     */
    bool switch_blocks(
        const std::vector<std::string> &disable_block_names,
        const std::vector<std::string> &enable_block_names, 
        const bool strict, 
        const bool force);
    

    /** \brief Set the set of enabled and disabled blocks.
     *
     * This is equivalent to calling \ref disable_blocks for all blocks, and
     * then calling \ref enable_blocks for a list of blocks,
     *
     * NOTE: This function does not provide a "force" option like \ref
     * enable_block or switch_blocks, because the only conflicts that are
     * possible are in the list of blocks to be enabled, and the caller should
     * know whether or not these are in conflict.
     *
     * \param strict Break on error if true, otherwise, try to enable all blocks
     * on the list even if some can't be enabled.
     */
    bool set_enabled_blocks(
        const std::vector<std::string> &enabled_block_names, 
        const bool strict);

    //\}

    ///////////////////////////////////////////////////////////////////////////
    //! \name Orocos RTT Hooks
    //\{


    virtual bool configureHook();

    virtual bool startHook();

    /* \brief Execute one iteration of the Scheme
     *
     * Read from hardware, compute estimation, compute control, and write to
     * hardware.
     */
    virtual void updateHook();

    //\}

    // TODO: ROS service call interface (make as a separate thing?)

  protected:

    /* \brief The last time updateHook was called.
     *
     * We maintain a single update time for all blocks so that any blocks
     * running at the same rate are executed in the same update() cycle.
     *
     */
    RTT::os::TimeService::nsecs last_update_time_;

    //! A list of block names (to distinguish from other peers)
    std::vector<std::string> block_names_;

    //! \name Graph structures
    //\{
    conman::graph::CausalGraph
      estimation_graph_,
      control_graph_;
    //\}

    //! \name Topologically-sorted structures
    //\{
    conman::graph::CausalOrdering
      estimation_serialization_,
      control_serialization_;
    //\}

    //! Maps blocks onto lists of conflicting blocks for quick access
    std::map<RTT::TaskContext*, std::vector<RTT::TaskContext*> > 
      block_conflicts_;

    /* \brief Connect a block to the appropriate blocks in a given graph
     *
     * This connects all inputs/outputs of block_a to all outputs/inputs of
     * block_b, given the groups, inputs, and outputs of block_b
     *
     * This is an internal function. For adding a block from
     * the public API, see \ref add_block.
     */
    static bool add_block_to_graph(
        RTT::TaskContext *new_block,
        conman::graph::CausalGraph &graph,
        conman::graph::CausalOrdering &ordering,
        const std::string &layer);

    /* \brief Generates an internal model of the RTT port connections
     *
     * This will generate a grah with RTT TaskContext blocks as vertices, and
     * RTT PortInterfaces as edges. 
     *
     * This only adds edges. Note that only edges between two blocks which have
     * already been added to the graph will be generated.
     */
    static bool regenerate_graph(
        conman::graph::CausalGraph &graph,
        conman::graph::CausalOrdering &ordering,
        const std::string &layer);
  };
}

#endif // ifndef __CONMAN_SCHEME_H
