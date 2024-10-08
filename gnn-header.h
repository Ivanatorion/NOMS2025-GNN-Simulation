#ifndef GNN_HEADER_H
#define GNN_HEADER_H

#include "ns3/header.h"
#include "ns3/nstime.h"

#define GNN_HEADER_SIZE 4

namespace ns3 {

class GNNHeader : public Header
{
public:
  GNNHeader ();

  /**
   * \param iter The GNN iteration number
   */
  void SetIter (uint32_t iter);
  /**
   * \return The GNN iteration number
   */
  uint32_t GetIter (void) const;
  /**
   * \param id The GNN node ID
   */
  void SetNodeID (uint8_t id);
  /**
   * \return The GNN iteration number
   */
  uint8_t GetNodeID (void) const;
  /**
   * \param hs The Hidden State of the sender
   */
  void SetHS (double hs[]);
  /**
   * \return The Hidden State of the sender
   */
  double* GetHS (void);

  bool IsValid (void) const;

  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);

  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);

private:
  bool isValid;
  uint8_t m_identifier;
  uint8_t m_nodeID;
  uint32_t m_iter; //!< Iteration Number
  double m_hiddenState[GNN_HEADER_SIZE]; //!< Hidden State
};

} // namespace ns3

#endif
