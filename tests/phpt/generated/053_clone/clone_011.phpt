--TEST--
object clone and property COW vector 011
--FILE--
<?php
class GeneratedClone011 { public $items; public function __construct($items) { $this->items = $items; } }
$original = new GeneratedClone011([14, 27]);
$copy = clone $original;
$copy->items[0] = 107;
echo implode(',', $original->items), ':', implode(',', $copy->items);
--EXPECT--
14,27:107,27
