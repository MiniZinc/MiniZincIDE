template <typename Cursor>
NodeVisitor<Cursor>::NodeVisitor(const Cursor &c) : m_cursor(c) {}

template <typename Cursor>
PreorderNodeVisitor<Cursor>::PreorderNodeVisitor(const Cursor &c)
    : NodeVisitor<Cursor>(c) {}

template <typename Cursor>
bool PreorderNodeVisitor<Cursor>::backtrack()
{
    while (!m_cursor.mayMoveSidewards() && m_cursor.mayMoveUpwards())
    {
        m_cursor.moveUpwards();
    }
    if (!m_cursor.mayMoveUpwards())
    {
        m_cursor.finalize();
        return false;
    }
    else
    {
        m_cursor.moveSidewards();
    }
    return true;
}

template <typename Cursor>
bool PreorderNodeVisitor<Cursor>::next()
{
    m_cursor.processCurrentNode();
    if (m_cursor.mayMoveDownwards())
    {
        m_cursor.moveDownwards();
    }
    else if (m_cursor.mayMoveSidewards())
    {
        m_cursor.moveSidewards();
    }
    else
    {
        return backtrack();
    }
    return true;
}

template <typename Cursor>
void PreorderNodeVisitor<Cursor>::run()
{
    while (next())
    {
    }
}

template <typename Cursor>
void PostorderNodeVisitor<Cursor>::moveToLeaf()
{
    while (m_cursor.mayMoveDownwards())
    {
        m_cursor.moveDownwards();
    }
}

template <typename Cursor>
PostorderNodeVisitor<Cursor>::PostorderNodeVisitor(const Cursor &c)
    : NodeVisitor<Cursor>(c)
{
    moveToLeaf();
}

template <class Cursor>
bool PostorderNodeVisitor<Cursor>::next()
{
    m_cursor.processCurrentNode();
    if (m_cursor.mayMoveSidewards())
    {
        m_cursor.moveSidewards();
        moveToLeaf();
    }
    else if (m_cursor.mayMoveUpwards())
    {
        m_cursor.moveUpwards();
    }
    else
    {
        m_cursor.finalize();
        return false;
    }
    return true;
}

template <typename Cursor>
void PostorderNodeVisitor<Cursor>::run()
{
    while (next())
    {
    }
}
