--TEST--
private and protected member access vector 014
--FILE--
<?php
class GeneratedVisibility014 {
    private $left = 102; protected $right = 76;
    private function left() { return $this->left; }
    protected function right() { return $this->right; }
    public function total() { return $this->left() + $this->right(); }
}
echo (new GeneratedVisibility014())->total();
--EXPECT--
178
