--TEST--
nested array copy-on-write vector 022
--FILE--
<?php
$tree = ['node' => ['value' => 89, 'items' => [90]]];
$copy = $tree;
$copy['node']['value'] += 3;
$copy['node']['items'][] = 94;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
89:92:90:90,94
