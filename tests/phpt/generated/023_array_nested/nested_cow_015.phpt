--TEST--
nested array copy-on-write vector 015
--FILE--
<?php
$tree = ['node' => ['value' => 61, 'items' => [62]]];
$copy = $tree;
$copy['node']['value'] += 3;
$copy['node']['items'][] = 66;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
61:64:62:62,66
