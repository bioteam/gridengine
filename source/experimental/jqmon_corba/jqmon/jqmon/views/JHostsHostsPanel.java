/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://www.gridengine.sunsource.net/license.html
 * 
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 * 
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 * 
 *   Copyright: 2001 by Sun Microsystems, Inc.
 * 
 *   All Rights Reserved.
 * 
 ************************************************************************/
/*___INFO__MARK_END__*/
package jqmon.views;

import java.awt.*;
import java.awt.event.*;


/* Swing 1.1 */
import javax.swing.*;
import javax.swing.event.*;
import javax.swing.border.*;

import java.util.*;

import javax.swing.table.*;

import jcodine.*;
import jqmon.*;
import jqmon.debug.*;
import jqmon.util.*;

public class JHostsHostsPanel extends JPanel {

	protected JDebug debug = null;
	protected static ResourceBundle messages = null;
	JGridBagLayout layout = null;
	HostsTableModel hostsModel = null;
	JTable table = null;
	JScrollPane scrollPane = null;
	
	public JHostsHostsPanel(JDebug d, JHostList hList){	
		super();
		layout = new JGridBagLayout();
		setLayout(layout);
		debug = d;
		Locale locale = Locale.getDefault();
		messages = ResourceBundle.getBundle("jqmon/views/MessageBundle", locale);
		initGUI(hList);
	}

	protected void initGUI(JHostList hList) {
		debug.DENTER("JHostsHostsPanel.initGUI");
		
		hostsModel =   new HostsTableModel(hList);
      JTable table = new JTable(hostsModel);
      scrollPane =   new JScrollPane(table);
      
		// try to put LongEditor
		final JLongPanel     longPanel  = new JLongPanel(0, 2, debug);
		final LongCellEditor longEditor = new LongCellEditor(longPanel);
		//table.setDefaultEditor(Long.class, longEditor);	
		table.setDefaultEditor(Integer.class, longEditor);
			
	  	table.setPreferredScrollableViewportSize(new Dimension(800, 800));
		layout.constrain(this, scrollPane, 1,1,1,1, GridBagConstraints.BOTH, GridBagConstraints.NORTH, 1.0, 1.0, 1,1,5,5 );

		debug.DEXIT();
	}
	
	public void setHostList(JHostList hostList) {
		hostsModel.setHostList(hostList);
	}

	public void updateHost(int index) {
		hostsModel.updateHost(index);
	}	

	public void deleteHost(int index) {
		debug.DENTER("JHostsHostsPanel.deleteQueue()"); 
      hostsModel.deleteHost(index);
		debug.DEXIT();
   }

	// inner class
	class JComponentCellRenderer implements TableCellRenderer {
      public Component getTableCellRendererComponent(JTable table, Object value, boolean isSelected, boolean hasFocus, int row, int column) {
            return (JComponent)value;
      }
  	}
}
